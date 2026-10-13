#!/bin/sh
# Fetches what the build needs into deps/, at the exact versions this port was
# made with (tools/versions.sh):
#   deps/Escape-Game-Cardputer   the game (src/, sim/include)
#   deps/M5GFX                   the graphics library the game draws with
# A folder already there is left as it is (a local copy can be put there, or
# pointed to with GAME_DIR and GFX_DIR when running make).
set -e

HERE=$(cd "$(dirname "$0")/.." && pwd)
. "$HERE/tools/versions.sh"
D=$HERE/deps
mkdir -p "$D"

clone() {  # clone <folder> <url> <tag or commit>
    if [ -d "$D/$1/.git" ]; then
        echo "deps/$1: already there"
        return
    fi
    git clone --quiet "$2" "$D/$1"
    git -C "$D/$1" -c advice.detachedHead=false checkout --quiet "$3"
    echo "deps/$1: $3"
}

clone Escape-Game-Cardputer https://github.com/RyoSaeba89/Escape-Game-Cardputer.git "$GAME_REF"
clone M5GFX https://github.com/m5stack/M5GFX.git "$M5GFX_REF"
