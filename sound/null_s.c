#include "stdafx.h"
#include "openttd.h"
#include "sound/null_s.h"

static const char *NullSoundStart(const char * const *parm) { return NULL; }
static void NullSoundStop(void) {}

const HalSoundDriver _null_sound_driver = {
	NullSoundStart,
	NullSoundStop,
};
