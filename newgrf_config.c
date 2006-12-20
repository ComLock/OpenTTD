/* $Id$ */

#include "stdafx.h"
#include "openttd.h"
#include "functions.h"
#include "macros.h"
#include "debug.h"
#include "variables.h"
#include "string.h"
#include "saveload.h"
#include "md5.h"
#include "network_data.h"
#include "newgrf.h"
#include "newgrf_config.h"

#include "fileio.h"
#include "fios.h"
#include <sys/types.h>
#include <sys/stat.h>

#ifdef WIN32
# include <io.h>
#else
# include <unistd.h>
# include <dirent.h>
#endif /* WIN32 */


GRFConfig *_all_grfs;
GRFConfig *_grfconfig;
GRFConfig *_grfconfig_newgame;
GRFConfig *_grfconfig_static;


/* Calculate the MD5 Sum for a GRF */
static bool CalcGRFMD5Sum(GRFConfig *config)
{
	FILE *f;
	char filename[MAX_PATH];
	md5_state_t md5state;
	md5_byte_t buffer[1024];
	size_t len;

	/* open the file */
	snprintf(filename, lengthof(filename), "%s%s", _paths.data_dir, config->filename);
	f = fopen(filename, "rb");
	if (f == NULL) return false;

	/* calculate md5sum */
	md5_init(&md5state);
	while ((len = fread(buffer, 1, sizeof(buffer), f)) != 0) {
		md5_append(&md5state, buffer, len);
	}
	md5_finish(&md5state, config->md5sum);

	fclose(f);

	return true;
}


/* Find the GRFID and calculate the md5sum */
bool FillGRFDetails(GRFConfig *config, bool is_static)
{
	if (!FioCheckFileExists(config->filename)) {
		SETBIT(config->flags, GCF_NOT_FOUND);
		return false;
	}

	/* Find and load the Action 8 information */
	/* 62 is the last file slot before sample.cat.
	 * Should perhaps be some "don't care" value */
	LoadNewGRFFile(config, 62, is_static ? GLS_SAFETYSCAN : GLS_FILESCAN);

	/* GCF_UNSAFE is set if GLS_SAFETYSCAN finds unsafe actions */
	if (HASBIT(config->flags, GCF_UNSAFE)) return false;

	/* Skip if the grfid is 0 (not read) or 0xFFFFFFFF (ttdp system grf) */
	if (config->grfid == 0 || config->grfid == 0xFFFFFFFF) return false;

	return CalcGRFMD5Sum(config);
}


void ClearGRFConfig(GRFConfig **config)
{
	/* GCF_COPY as in NOT strdupped/alloced the filename, name and info */
	if (!HASBIT((*config)->flags, GCF_COPY)) {
		free((*config)->filename);
		free((*config)->name);
		free((*config)->info);
	}
	free(*config);
	*config = NULL;
}


/* Clear a GRF Config list */
void ClearGRFConfigList(GRFConfig **config)
{
	GRFConfig *c, *next;
	for (c = *config; c != NULL; c = next) {
		next = c->next;
		ClearGRFConfig(&c);
	}
	*config = NULL;
}


/** Copy a GRF Config list
 * @param dst pointer to destination list
 * @param srt pointer to source list values
 * @return pointer to the last value added to the destination list */
GRFConfig **CopyGRFConfigList(GRFConfig **dst, const GRFConfig *src)
{
	GRFConfig *c;

	for (; src != NULL; src = src->next) {
		c = calloc(1, sizeof(*c));
		*c = *src;
		c->filename = strdup(src->filename);
		if (src->name != NULL) c->name = strdup(src->name);
		if (src->info != NULL) c->info = strdup(src->info);

		*dst = c;
		dst = &c->next;
	}

	return dst;
}


/* Reset the current GRF Config to either blank or newgame settings */
void ResetGRFConfig(bool defaults)
{
	GRFConfig **c = &_grfconfig;

	ClearGRFConfigList(c);

	if (defaults) c = CopyGRFConfigList(c, _grfconfig_newgame);
	CopyGRFConfigList(c, _grfconfig_static);
}


/* Check if all GRFs in the GRF Config can be loaded */
bool IsGoodGRFConfigList(void)
{
	bool res = true;
	GRFConfig *c;

	for (c = _grfconfig; c != NULL; c = c->next) {
		const GRFConfig *f = FindGRFConfig(c->grfid, c->md5sum);
		if (f == NULL) {
			char buf[512], *p = buf;
			uint i;

			p += snprintf(p, lastof(buf) - p, "Couldn't find NewGRF %08X (%s) checksum ", BSWAP32(c->grfid), c->filename);
			for (i = 0; i < lengthof(c->md5sum); i++) {
				p += snprintf(p, lastof(buf) - p, "%02X", c->md5sum[i]);
			}
			ShowInfo(buf);

			res = false;
		} else {
			DEBUG(grf, 1) ("[GRF] Loading GRF %08X from %s", BSWAP32(c->grfid), f->filename);
			c->filename = strdup(f->filename);
			c->name     = strdup(f->name);
			c->info     = strdup(f->info);
		}
	}

	return res;
}


extern bool FiosIsValidFile(const char *path, const struct dirent *ent, struct stat *sb);

/* Scan a path for NewGRFs */
static uint ScanPath(const char *path)
{
	uint num = 0;
	struct stat sb;
	struct dirent *dirent;
	DIR *dir;
	GRFConfig *c;

	if ((dir = opendir(path)) == NULL) return 0;

	while ((dirent = readdir(dir)) != NULL) {
		const char *d_name = FS2OTTD(dirent->d_name);
		char filename[MAX_PATH];

		if (!FiosIsValidFile(path, dirent, &sb)) continue;

		snprintf(filename, lengthof(filename), "%s" PATHSEP "%s", path, d_name);

		if (sb.st_mode & S_IFDIR) {
			/* Directory */
			if (strcmp(d_name, ".") == 0 || strcmp(d_name, "..") == 0) continue;
			num += ScanPath(filename);
		} else if (sb.st_mode & S_IFREG) {
			/* File */
			char *ext = strrchr(filename, '.');
			char *file = filename + strlen(_paths.data_dir) + 1; // Crop base path

			/* If no extension or extension isn't .grf, skip the file */
			if (ext == NULL) continue;
			if (strcasecmp(ext, ".grf") != 0) continue;

			c = calloc(1, sizeof(*c));
			c->filename = strdup(file);

			if (FillGRFDetails(c, false)) {
				if (_all_grfs == NULL) {
					_all_grfs = c;
				} else {
					/* Insert file into list at a position determined by its
					 * name, so the list is sorted as we go along */
					GRFConfig **pd, *d;
					for (pd = &_all_grfs; (d = *pd) != NULL; pd = &d->next) {
						if (strcasecmp(c->name, d->name) <= 0) break;
					}
					c->next = d;
					*pd = c;
				}

				num++;
			} else {
				/* File couldn't be opened, or is either not a NewGRF or is a
				 * 'system' NewGRF, so forget about it. */
				free(c->filename);
				free(c->name);
				free(c->info);
				free(c);
			}
		}
	}

	closedir(dir);

	return num;
}


/* Scan for all NewGRFs */
void ScanNewGRFFiles(void)
{
	uint num;

	ClearGRFConfigList(&_all_grfs);

	DEBUG(grf, 1) ("[GRF] Scanning for NewGRFs");
	num = ScanPath(_paths.data_dir);
	DEBUG(grf, 1) ("[GRF] Scan complete, found %d files", num);
}


/* Find a NewGRF in the scanned list */
const GRFConfig *FindGRFConfig(uint32 grfid, uint8 *md5sum)
{
	GRFConfig *c;
	static const uint8 blanksum[sizeof(c->md5sum)] = { 0 };

	for (c = _all_grfs; c != NULL; c = c->next) {
		if (c->grfid == grfid) {
			if (memcmp(blanksum, c->md5sum, sizeof(c->md5sum)) == 0) CalcGRFMD5Sum(c);
			if (memcmp(md5sum, c->md5sum, sizeof(c->md5sum)) == 0) return c;
		}
	}

	return NULL;
}

/** Structure for UnknownGRFs; this is a lightweight variant of GRFConfig */
typedef struct UnknownGRF UnknownGRF;
struct UnknownGRF {
	UnknownGRF *next;
	uint32 grfid;
	uint8  md5sum[16];
	char   name[NETWORK_GRF_NAME_LENGTH];
};

/**
 * Finds the name of a NewGRF in the list of names for unknown GRFs. An
 * unknown GRF is a GRF where the .grf is not found during scanning.
 *
 * The names are resolved via UDP calls to servers that should know the name,
 * though the replies may not come. This leaves "<Unknown>" as name, though
 * that shouldn't matter _very_ much as they need GRF crawler or so to look
 * up the GRF anyway and that works better with the GRF ID.
 *
 * @param grfid  the GRF ID part of the 'unique' GRF identifier
 * @param md5sum the MD5 checksum part of the 'unique' GRF identifier
 * @param create whether to create a new GRFConfig if the GRFConfig did not
 *               exist in the fake list of GRFConfigs.
 * @return the GRFConfig with the given GRF ID and MD5 checksum or NULL when
 *         it does not exist and create is false. This value must NEVER be
 *         freed by the caller.
 */
char *FindUnknownGRFName(uint32 grfid, uint8 *md5sum, bool create)
{
	UnknownGRF *grf;
	static UnknownGRF *unknown_grfs = NULL;

	for (grf = unknown_grfs; grf != NULL; grf = grf->next) {
		if (grf->grfid == grfid) {
			if (memcmp(md5sum, grf->md5sum, sizeof(grf->md5sum)) == 0) return grf->name;
		}
	}

	if (!create) return NULL;

	grf = calloc(1, sizeof(*grf));
	grf->grfid = grfid;
	grf->next  = unknown_grfs;
	ttd_strlcpy(grf->name, UNKNOWN_GRF_NAME_PLACEHOLDER, sizeof(grf->name));
	memcpy(grf->md5sum, md5sum, sizeof(grf->md5sum));

	unknown_grfs = grf;
	return grf->name;
}


/* Retrieve a NewGRF from the current config by its grfid */
GRFConfig *GetGRFConfig(uint32 grfid)
{
	GRFConfig *c;

	for (c = _grfconfig; c != NULL; c = c->next) {
		if (c->grfid == grfid) return c;
	}

	return NULL;
}


/* Build a space separated list of parameters, and terminate */
char *GRFBuildParamList(char *dst, const GRFConfig *c, const char *last)
{
	uint i;

	/* Return an empty string if there are no parameters */
	if (c->num_params == 0) return strecpy(dst, "", last);

	for (i = 0; i < c->num_params; i++) {
		if (i > 0) dst = strecpy(dst, " ", last);
		dst += snprintf(dst, last - dst, "%d", c->param[i]);
	}
	return dst;
}


static const SaveLoad _grfconfig_desc[] = {
	SLE_STR(GRFConfig, filename,   SLE_STR, 0x40),
	SLE_VAR(GRFConfig, grfid,      SLE_UINT32),
	SLE_ARR(GRFConfig, md5sum,     SLE_UINT8, 16),
	SLE_ARR(GRFConfig, param,      SLE_UINT32, 0x80),
	SLE_VAR(GRFConfig, num_params, SLE_UINT8),
	SLE_END()
};


static void Save_NGRF(void)
{
	GRFConfig *c;
	int index = 0;

	for (c = _grfconfig; c != NULL; c = c->next) {
		if (HASBIT(c->flags, GCF_STATIC)) continue;
		SlSetArrayIndex(index++);
		SlObject(c, _grfconfig_desc);
	}
}


static void Load_NGRF(void)
{
	GRFConfig *first = NULL;
	GRFConfig **last = &first;

	while (SlIterateArray() != -1) {
		GRFConfig *c = calloc(1, sizeof(*c));
		SlObject(c, _grfconfig_desc);

		/* Append our configuration to the list */
		*last = c;
		last = &c->next;
	}

	/* Append static NewGRF configuration */
	CopyGRFConfigList(last, _grfconfig_static);

	ClearGRFConfigList(&_grfconfig);
	_grfconfig = first;
}

const ChunkHandler _newgrf_chunk_handlers[] = {
	{ 'NGRF', Save_NGRF, Load_NGRF, CH_ARRAY | CH_LAST }
};

