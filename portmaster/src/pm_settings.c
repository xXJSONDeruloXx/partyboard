#include "port/settings.h"
#include <stdlib.h>
#include <string.h>

CARDFileType partyboard_settings_card_file_type(void) { return CARD_RAWIMAGE; }
bool partyboard_settings_enableTurboKeybind(void) { return false; }
bool partyboard_settings_skipBootSequence(void)
{
    const char *value = getenv("PARTYBOARD_SKIP_BOOT");
    return value != NULL && strcmp(value, "0") != 0;
}
bool partyboard_settings_unlock_all_minigames(void) { return false; }
bool partyboard_settings_unlock_bowsers_gnarly_party(void) { return false; }
