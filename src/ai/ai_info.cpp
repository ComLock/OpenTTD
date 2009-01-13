/* $Id$ */

/** @file ai_info.cpp Implementation of AIFileInfo */

#include "../stdafx.h"
#include "../core/alloc_func.hpp"

#include <squirrel.h>
#include "../script/squirrel.hpp"
#include "../script/squirrel_helper.hpp"
#include "../script/squirrel_class.hpp"
#include "../script/squirrel_std.hpp"
#include "ai.hpp"
#include "ai_info.hpp"
#include "ai_scanner.hpp"
#include "api/ai_controller.hpp"
#include "../settings_type.h"
#include "../openttd.h"

AIFileInfo::~AIFileInfo()
{
	this->engine->ReleaseObject(this->SQ_instance);
	free((void *)this->author);
	free((void *)this->name);
	free((void *)this->description);
	free((void *)this->date);
	free((void *)this->instance_name);
	free(this->script_name);
	free(this->dir_name);
	free(this->SQ_instance);
}

const char *AIFileInfo::GetAuthor()
{
	if (this->author == NULL) this->author = this->engine->CallStringMethodStrdup(*this->SQ_instance, "GetAuthor");
	return this->author;
}

const char *AIFileInfo::GetName()
{
	if (this->name == NULL) this->name = this->engine->CallStringMethodStrdup(*this->SQ_instance, "GetName");
	return this->name;
}

const char *AIFileInfo::GetDescription()
{
	if (this->description == NULL) this->description = this->engine->CallStringMethodStrdup(*this->SQ_instance, "GetDescription");
	return this->description;
}

int AIFileInfo::GetVersion()
{
	return this->engine->CallIntegerMethod(*this->SQ_instance, "GetVersion");
}

void AIFileInfo::GetSettings()
{
	this->engine->CallMethod(*this->SQ_instance, "GetSettings", NULL, -1);
}

const char *AIFileInfo::GetDate()
{
	if (this->date == NULL) this->date = this->engine->CallStringMethodStrdup(*this->SQ_instance, "GetDate");
	return this->date;
}

const char *AIFileInfo::GetInstanceName()
{
	if (this->instance_name == NULL) this->instance_name = this->engine->CallStringMethodStrdup(*this->SQ_instance, "CreateInstance");
	return this->instance_name;
}

bool AIFileInfo::CanLoadFromVersion(int version)
{
	if (version == -1) return true;
	if (!this->engine->MethodExists(*this->SQ_instance, "CanLoadFromVersion")) return true;

	HSQUIRRELVM vm = this->engine->GetVM();
	int top = sq_gettop(vm);

	sq_pushobject(vm, *this->SQ_instance);
	sq_pushstring(vm, OTTD2FS("CanLoadFromVersion"), -1);
	sq_get(vm, -2);
	sq_pushobject(vm, *this->SQ_instance);
	sq_pushinteger(vm, version);
	sq_call(vm, 2, SQTrue, SQFalse);

	HSQOBJECT ret;
	sq_getstackobj(vm, -1, &ret);

	sq_settop(vm, top);
	return sq_objtobool(&ret);
}

const char *AIFileInfo::GetDirName()
{
	return this->dir_name;
}

const char *AIFileInfo::GetScriptName()
{
	return this->script_name;
}

void AIFileInfo::CheckMethods(SQInteger *res, const char *name)
{
	if (!this->engine->MethodExists(*this->SQ_instance, name)) {
		char error[1024];
		snprintf(error, sizeof(error), "your AIFileInfo doesn't have the method '%s'", name);
		this->engine->ThrowError(error);
		*res = SQ_ERROR;
	}
}

/* static */ SQInteger AIFileInfo::Constructor(HSQUIRRELVM vm, AIFileInfo *info)
{
	SQInteger res = 0;

	/* Set some basic info from the parent */
	info->SQ_instance = MallocT<SQObject>(1);
	Squirrel::GetInstance(vm, info->SQ_instance, 2);
	/* Make sure the instance stays alive over time */
	sq_addref(vm, info->SQ_instance);
	info->base = ((AIScanner *)Squirrel::GetGlobalPointer(vm));
	info->engine = info->base->GetEngine();

	/* Check if all needed fields are there */
	info->CheckMethods(&res, "GetAuthor");
	info->CheckMethods(&res, "GetName");
	info->CheckMethods(&res, "GetDescription");
	info->CheckMethods(&res, "GetVersion");
	info->CheckMethods(&res, "GetDate");
	info->CheckMethods(&res, "CreateInstance");

	/* Abort if one method was missing */
	if (res != 0) return res;

	info->script_name = strdup(info->base->GetCurrentScript());
	info->dir_name = strdup(info->base->GetCurrentDirName());

	return 0;
}

/* static */ SQInteger AIInfo::Constructor(HSQUIRRELVM vm)
{
	/* Get the AIInfo */
	SQUserPointer instance;
	sq_getinstanceup(vm, 2, &instance, 0);
	AIInfo *info = (AIInfo *)instance;

	SQInteger res = AIFileInfo::Constructor(vm, info);
	if (res != 0) return res;

	AIConfigItem config;
	config.name = strdup("start_date");
	config.description = strdup("The amount of months after the start of the last AI, this AI will start (give or take).");
	config.min_value = 0;
	config.max_value = 120;
	config.easy_value = 48;
	config.medium_value = 24;
	config.hard_value = 12;
	config.custom_value = 24;
	config.flags = AICONFIG_NONE;
	info->config_list.push_back(config);

	/* Check if we have settings */
	if (info->engine->MethodExists(*info->SQ_instance, "GetSettings")) {
		info->GetSettings();
	}

	/* Remove the link to the real instance, else it might get deleted by RegisterAI() */
	sq_setinstanceup(vm, 2, NULL);
	/* Register the AI to the base system */
	info->base->RegisterAI(info);
	return 0;
}

/* static */ SQInteger AIInfo::DummyConstructor(HSQUIRRELVM vm)
{
	/* Get the AIInfo */
	SQUserPointer instance;
	sq_getinstanceup(vm, 2, &instance, 0);
	AIInfo *info = (AIInfo *)instance;

	SQInteger res = AIFileInfo::Constructor(vm, info);
	if (res != 0) return res;

	/* Remove the link to the real instance, else it might get deleted by RegisterAI() */
	sq_setinstanceup(vm, 2, NULL);
	/* Register the AI to the base system */
	info->base->SetDummyAI(info);
	return 0;
}

AIInfo::~AIInfo()
{
	/* Free all allocated strings */
	for (AIConfigItemList::iterator it = this->config_list.begin(); it != this->config_list.end(); it++) {
		free((char *)(*it).name);
		free((char *)(*it).description);
	}
	this->config_list.clear();
}

SQInteger AIInfo::AddSetting(HSQUIRRELVM vm)
{
	AIConfigItem config;
	memset(&config, 0, sizeof(config));
	int items = 0;

	/* Read the table, and find all properties we care about */
	sq_pushnull(vm);
	while (SQ_SUCCEEDED(sq_next(vm, -2))) {
		const SQChar *sqkey;
		sq_getstring(vm, -2, &sqkey);
		const char *key = FS2OTTD(sqkey);

		if (strcmp(key, "name") == 0) {
			const SQChar *sqvalue;
			sq_getstring(vm, -1, &sqvalue);
			config.name = strdup(FS2OTTD(sqvalue));
			char *s;
			/* Don't allow '=' and ',' in configure setting names, as we need those
			 *  2 chars to nicely store the settings as a string. */
			while ((s = (char *)strchr(config.name, '=')) != NULL) *s = '_';
			while ((s = (char *)strchr(config.name, ',')) != NULL) *s = '_';
			items |= 0x001;
		} else if (strcmp(key, "description") == 0) {
			const SQChar *sqdescription;
			sq_getstring(vm, -1, &sqdescription);
			config.description = strdup(FS2OTTD(sqdescription));
			items |= 0x002;
		} else if (strcmp(key, "min_value") == 0) {
			SQInteger res;
			sq_getinteger(vm, -1, &res);
			config.min_value = res;
			items |= 0x004;
		} else if (strcmp(key, "max_value") == 0) {
			SQInteger res;
			sq_getinteger(vm, -1, &res);
			config.max_value = res;
			items |= 0x008;
		} else if (strcmp(key, "easy_value") == 0) {
			SQInteger res;
			sq_getinteger(vm, -1, &res);
			config.easy_value = res;
			items |= 0x010;
		} else if (strcmp(key, "medium_value") == 0) {
			SQInteger res;
			sq_getinteger(vm, -1, &res);
			config.medium_value = res;
			items |= 0x020;
		} else if (strcmp(key, "hard_value") == 0) {
			SQInteger res;
			sq_getinteger(vm, -1, &res);
			config.hard_value = res;
			items |= 0x040;
		} else if (strcmp(key, "custom_value") == 0) {
			SQInteger res;
			sq_getinteger(vm, -1, &res);
			config.custom_value = res;
			items |= 0x080;
		} else if (strcmp(key, "flags") == 0) {
			SQInteger res;
			sq_getinteger(vm, -1, &res);
			config.flags = (AIConfigFlags)res;
			items |= 0x100;
		} else {
			char error[1024];
			snprintf(error, sizeof(error), "unknown setting property '%s'", key);
			this->engine->ThrowError(error);
			return SQ_ERROR;
		}

		sq_pop(vm, 2);
	}
	sq_pop(vm, 1);

	/* Make sure all properties are defined */
	if (items != 0x1FF) {
		char error[1024];
		snprintf(error, sizeof(error), "please define all properties of a setting");
		this->engine->ThrowError(error);
		return SQ_ERROR;
	}

	this->config_list.push_back(config);
	return 0;
}

const AIConfigItemList *AIInfo::GetConfigList()
{
	return &this->config_list;
}

int AIInfo::GetSettingDefaultValue(const char *name)
{
	for (AIConfigItemList::iterator it = this->config_list.begin(); it != this->config_list.end(); it++) {
		if (strcmp((*it).name, name) != 0) continue;
		/* The default value depends on the difficulty level */
		switch ((_game_mode == GM_MENU) ? _settings_newgame.difficulty.diff_level : _settings_game.difficulty.diff_level) {
			case 0: return (*it).easy_value;
			case 1: return (*it).medium_value;
			case 2: return (*it).hard_value;
			case 3: return (*it).custom_value;
			default: NOT_REACHED();
		}
	}

	/* There is no such setting */
	return -1;
}

/* static */ SQInteger AILibrary::Constructor(HSQUIRRELVM vm)
{
	/* Create a new AIFileInfo */
	AILibrary *library = new AILibrary();

	SQInteger res = AIFileInfo::Constructor(vm, library);
	if (res != 0) return res;

	/* Register the Library to the base system */
	library->base->RegisterLibrary(library);

	return 0;
}

/* static */ SQInteger AILibrary::Import(HSQUIRRELVM vm)
{
	SQConvert::SQAutoFreePointers ptr;
	const char *library = GetParam(SQConvert::ForceType<const char *>(), vm, 2, &ptr);
	const char *class_name = GetParam(SQConvert::ForceType<const char *>(), vm, 3, &ptr);
	int version = GetParam(SQConvert::ForceType<int>(), vm, 4, &ptr);

	if (!AI::ImportLibrary(library, class_name, version, vm)) return -1;
	return 1;
}
