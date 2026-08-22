#!/bin/bash
# PORTMASTER: partyboard, Party Board.sh

XDG_DATA_HOME=${XDG_DATA_HOME:-$HOME/.local/share}

if [ -d "/opt/system/Tools/PortMaster/" ]; then
    controlfolder="/opt/system/Tools/PortMaster"
elif [ -d "/opt/tools/PortMaster/" ]; then
    controlfolder="/opt/tools/PortMaster"
elif [ -d "$XDG_DATA_HOME/PortMaster/" ]; then
    controlfolder="$XDG_DATA_HOME/PortMaster"
else
    controlfolder="/roms/ports/PortMaster"
fi

source "$controlfolder/control.txt"
get_controls

GAMEDIR="$(CDPATH= cd -- "$(dirname -- "$0")" && pwd -P)"
cd "$GAMEDIR" || exit 1

export SDL_GAMECONTROLLERCONFIG="$sdl_controllerconfig"

$GPTOKEYB partyboard-portmaster -c partyboard-portmaster.gptk &
GPTOKEYB_PID=$!

./partyboard-portmaster "$GAMEDIR" 2>&1 | tee "$GAMEDIR/log.txt"
GAME_STATUS=${PIPESTATUS[0]}

kill "$GPTOKEYB_PID" 2>/dev/null || true
if [ -n "$CUR_TTY" ]; then
    printf "\033c" >> "$CUR_TTY"
fi
if command -v systemctl >/dev/null 2>&1; then
    systemctl restart oga_events >/dev/null 2>&1 &
fi

exit "$GAME_STATUS"
