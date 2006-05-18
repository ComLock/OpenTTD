/* $Id$ */

/** @file
 * Implementation of  Action 04 "universal holder" structure and functions.
 * This file implements a linked-lists of strings,
 * holding everything that the newgrf action 04 will send over to OpenTTD.
 * One of the biggest problems is that Dynamic lang Array uses ISO codes
 * as way to identifying current user lang, while newgrf uses bit shift codes
 * not related to ISO.  So equivalence functionnality had to be set.
 */

#include "stdafx.h"
#include "debug.h"
#include "openttd.h"
#include "string.h"
#include "strings.h"
#include "variables.h"
#include "macros.h"
#include "table/strings.h"
#include "newgrf_text.h"

#define GRFTAB  28
#define TABSIZE 11

/**
 * Explains the newgrf shift bit positionning.
 * the grf base will not be used in order to find the string, but rather for
 * jumping from standard langID scheme to the new one.
 */
typedef enum grf_base_languages {
	GRFLB_AMERICAN    = 0x01,
	GRFLB_ENGLISH     = 0x02,
	GRFLB_GERMAN      = 0x04,
	GRFLB_FRENCH      = 0x08,
	GRFLB_SPANISH     = 0x10,
	GRFLB_GENERIC     = 0x80,
} grf_base_language;

typedef enum grf_extended_languages {
	GRFLX_AMERICAN    = 0x00,
	GRFLX_ENGLISH     = 0x01,
	GRFLX_GERMAN      = 0x02,
	GRFLX_FRENCH      = 0x03,
	GRFLX_SPANISH     = 0x04,
	GRFLX_RUSSIAN     = 0x07,
	GRFLX_CZECH       = 0x15,
	GRFLX_SLOVAK      = 0x16,
	GRFLX_AFRIKAANS   = 0x1B,
	GRFLX_GREEK       = 0x1E,
	GRFLX_DUTCH       = 0x1F,
	GRFLX_CATALAN     = 0x22,
	GRFLX_HUNGARIAN   = 0x24,
	GRFLX_ITALIAN     = 0x27,
	GRFLX_ROMANIAN    = 0x28,
	GRFLX_ICELANDIC   = 0x29,
	GRFLX_LATVIAN     = 0x2A,
	GRFLX_LITHUANIAN  = 0x2B,
	GRFLX_SLOVENIAN   = 0x2C,
	GRFLX_DANISH      = 0x2D,
	GRFLX_SWEDISH     = 0x2E,
	GRFLX_NORWEGIAN   = 0x2F,
	GRFLX_POLISH      = 0x30,
	GRFLX_GALICIAN    = 0x31,
	GRFLX_FRISIAN     = 0x32,
	GRFLX_UKRAINIAN   = 0x33,
	GRFLX_ESTONIAN    = 0x34,
	GRFLX_FINNISH     = 0x35,
	GRFLX_PORTUGUESE  = 0x36,
	GRFLX_BRAZILIAN   = 0x37,
	GRFLX_CROATIAN    = 0x38,
	GRFLX_TURKISH     = 0x3E,
	GRFLX_UNSPECIFIED = 0x7F,
} grf_language;


typedef struct iso_grf {
	char code[6];
	byte grfLangID;
} iso_grf;

/**
 * ISO code VS NewGrf langID conversion array.
 * This array is used in two ways:
 * 1-its ISO part is matching OpenTTD dynamic language id
 *   with newgrf bit positionning language id
 * 2-its shift part is used to know what is the shift to
 *   watch for when inserting new strings, hence analysing newgrf langid
 */
const iso_grf iso_codes[] = {
	{"en_US", GRFLX_AMERICAN},
	{"en_GB", GRFLX_ENGLISH},
	{"de_DE", GRFLX_GERMAN},
	{"fr_FR", GRFLX_FRENCH},
	{"es_ES", GRFLX_SPANISH},
	{"af_ZA", GRFLX_AFRIKAANS},
	{"hr_HR", GRFLX_CROATIAN},
	{"cs_CS", GRFLX_CZECH},
	{"ca_ES", GRFLX_CATALAN},
	{"da_DA", GRFLX_DANISH},
	{"nl_NL", GRFLX_DUTCH},
	{"et_ET", GRFLX_ESTONIAN},
	{"fi_FI", GRFLX_FINNISH},
	{"fy_NL", GRFLX_FRISIAN},
	{"gl_ES", GRFLX_GALICIAN},
	{"el_GR", GRFLX_GREEK},
	{"hu_HU", GRFLX_HUNGARIAN},
	{"is_IS", GRFLX_ICELANDIC},
	{"it_IT", GRFLX_ITALIAN},
	{"lv_LV", GRFLX_LATVIAN},
	{"lt_LT", GRFLX_LITHUANIAN},
	{"nb_NO", GRFLX_NORWEGIAN},
	{"pl_PL", GRFLX_POLISH},
	{"pt_PT", GRFLX_PORTUGUESE},
	{"pt_BR", GRFLX_BRAZILIAN},
	{"ro_RO", GRFLX_ROMANIAN},
	{"ru_RU", GRFLX_RUSSIAN},
	{"sk_SK", GRFLX_SLOVAK},
	{"sl_SL", GRFLX_SLOVENIAN},
	{"sv_SE", GRFLX_SWEDISH},
	{"tr_TR", GRFLX_TURKISH},
	{"uk_UA", GRFLX_UKRAINIAN},
	{"gen",   GRFLB_GENERIC}   //this is not iso code, but there has to be something...
};


static uint _num_grf_texts = 0;
static GRFTextEntry _grf_text[(1 << TABSIZE) * 3];
static byte _currentLangID = GRFLX_ENGLISH;  //by default, english is used.


static void TranslateTTDPatchCodes(char *str)
{
	char *c;

	for (c = str; *c != '\0'; c++) {
		switch ((byte)*c) {
			case 0x01: c++; break;
			case 0x0D: *c = 10; break;
			case 0x0E: *c = 8; break;
			case 0x0F: *c = 9; break;
			case 0x1F: *c = 2; c += 2; break;
			case 0x7B:
			case 0x7C:
			case 0x7D:
			case 0x7E: *c = 0x8E; break;
			case 0x81: c += 2; break;
			case 0x85: *c = 0x86; break;
			case 0x88: *c = 15; break;
			case 0x89: *c = 16; break;
			case 0x8A: *c = 17; break;
			case 0x8B: *c = 18; break;
			case 0x8C: *c = 19; break;
			case 0x8D: *c = 20; break;
			case 0x8E: *c = 21; break;
			case 0x8F: *c = 22; break;
			case 0x90: *c = 23; break;
			case 0x91: *c = 24; break;
			case 0x92: *c = 25; break;
			case 0x93: *c = 26; break;
			case 0x94: *c = 27; break;
			case 0x95: *c = 28; break;
			case 0x96: *c = 29; break;
			case 0x97: *c = 30; break;
			case 0x98: *c = 31; break;
			default:
				/* Validate any unhandled character */
				if (!IsValidAsciiChar(*c)) *c = '?';
				break;
		}
	}
}


/**
 * Add the new read string into our structure.
 */
StringID AddGRFString(uint32 grfid, uint16 stringid, byte langid_to_add, bool new_scheme, const char *text_to_add, StringID def_string)
{
	GRFText *newtext;
	uint id;

	/* When working with the old language scheme (grf_version is less than 7) and
	 * English or American is among the set bits, simply add it as English in
	 * the new scheme, i.e. as langid = 1.
	 * If English is set, it is pretty safe to assume the translations are not
	 * actually translated.
	 */
	if (!new_scheme) {
		if (HASBITS(langid_to_add, GRFLB_AMERICAN | GRFLB_ENGLISH)) {
			langid_to_add = GRFLX_ENGLISH;
		} else {
			StringID ret = STR_EMPTY;
			if (langid_to_add & GRFLB_GERMAN)  ret = AddGRFString(grfid, stringid, 1 << 6 | GRFLX_GERMAN,  true, text_to_add, def_string);
			if (langid_to_add & GRFLB_FRENCH)  ret = AddGRFString(grfid, stringid, 1 << 6 | GRFLX_FRENCH,  true, text_to_add, def_string);
			if (langid_to_add & GRFLB_SPANISH) ret = AddGRFString(grfid, stringid, 1 << 6 | GRFLX_SPANISH, true, text_to_add, def_string);
			return ret;
		}
	}

	for (id = 0; id < _num_grf_texts; id++) {
		if (_grf_text[id].grfid == grfid && _grf_text[id].stringid == stringid) {
			break;
		}
	}

	/* Too many strings allocated, return empty */
	if (id == lengthof(_grf_text)) return STR_EMPTY;

	newtext = calloc(1, sizeof(*newtext));
	newtext->langid = GB(langid_to_add, 0, 6);
	newtext->text   = strdup(text_to_add);
	newtext->next   = NULL;

	TranslateTTDPatchCodes(newtext->text);

	/* If we didn't find our stringid and grfid in the list, allocate a new id */
	if (id == _num_grf_texts) _num_grf_texts++;

	if (_grf_text[id].textholder == NULL) {
		_grf_text[id].grfid      = grfid;
		_grf_text[id].stringid   = stringid;
		_grf_text[id].def_string = def_string;
		_grf_text[id].textholder = newtext;
	} else {
		GRFText *textptr = _grf_text[id].textholder;
		while (textptr->next != NULL) textptr = textptr->next;
		textptr->next = newtext;
	}

	DEBUG(grf, 2)("Added 0x%X: grfid 0x%X string 0x%X lang 0x%X string %s", id, grfid, stringid, newtext->langid, newtext->text);

	return (GRFTAB << TABSIZE) + id;
}


/**
 * Returns the index for this stringid associated with its grfID
 */
StringID GetGRFStringID(uint32 grfid, uint16 stringid)
{
	uint id;
	for (id = 0; id < _num_grf_texts; id++) {
		if (_grf_text[id].grfid == grfid && _grf_text[id].stringid == stringid) {
			return (GRFTAB << TABSIZE) + id;
		}
	}

	return STR_UNDEFINED;
}


char *GetGRFString(char *buff, uint16 stringid)
{
	GRFText *search_text;
	GRFText *default_text = NULL;

	assert(_grf_text[stringid].grfid != 0);
	/*Search the list of lang-strings of this stringid for current lang */
	for (search_text = _grf_text[stringid].textholder; search_text != NULL; search_text = search_text->next) {
		if (search_text->langid == _currentLangID) {
			return strecpy(buff, search_text->text, NULL);
		}

		/* If the current string is English or American, set it as the
		 * fallback language if the specific language isn't available. */
		if (search_text->langid == GRFLX_ENGLISH || search_text->langid == GRFLX_AMERICAN) {
			default_text = search_text;
		}
	}

	/* If there is a fallback string, return that */
	if (default_text != NULL) return strecpy(buff, default_text->text, NULL);

	/* Use the default string ID if the fallback string isn't available */
	return GetString(buff, _grf_text[stringid].def_string);
}

/**
 * Equivalence Setter function between game and newgrf langID.
 * This function will adjust _currentLangID as to what is the LangID
 * of the current language set by the user.
 * The array iso_codes will be used to find that match.
 * If not found, it will have to be standard english
 * This function is called after the user changed language,
 * from strings.c:ReadLanguagePack
 * @param iso code of current selection
 */
void SetCurrentGrfLangID(const char *iso_name)
{
	byte ret,i;

	/* Use English by default, if we can't match up the iso_code. */
	ret = GRFLX_ENGLISH;

	for (i=0; i < lengthof(iso_codes); i++) {
		if (strncmp(iso_codes[i].code, iso_name, strlen(iso_codes[i].code)) == 0) {
			/* We found a match, so let's use it. */
			ret = i;
			break;
		}
	}
	_currentLangID = ret;
}

/**
 * House cleaning.
 * Remove all strings and reset the text counter.
 */
void CleanUpStrings(void)
{
	uint id;

	for (id = 0; id < _num_grf_texts; id++) {
		GRFText *grftext = _grf_text[id].textholder;
		while (grftext != NULL) {
			GRFText *grftext2 = grftext->next;
			free(grftext->text);
			free(grftext);
			grftext = grftext2;
		}
		_grf_text[id].grfid      = 0;
		_grf_text[id].stringid   = 0;
		_grf_text[id].textholder = NULL;
	}

	_num_grf_texts = 0;
}
