#include "dolphin/types.h"
#include "version.h"
#include "port/port_version.h"

u8 partyboard_version_get_version(void) { return VERSION_NO_ENG1; }
BOOL partyboard_version_is_pal(void) { return FALSE; }
BOOL partyboard_version_is_usa(void) { return TRUE; }
BOOL partyboard_version_is_ntsc(void) { return TRUE; }
