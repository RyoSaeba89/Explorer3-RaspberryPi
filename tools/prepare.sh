#!/bin/sh
# Copies of the game's sources, changed for the Pi (the game itself is never
# modified), into $OUT (build/gen):
#   main.cpp          tools/pi-game.sed, then src/bridge.inc appended
#   textes.h          the keyboard's texts: tools/pi-texts.sed
#   textes_xbox.h     the gamepad's (tools/pad-texts.sed), with A B X Y...
#   textes_ps.h       ... or × ○ □ △; namespaces textes_xbox, textes_ps
#   texts_table.inc   the three versions of each text side by side (bridge.inc)
#   mots.h dessins.h diffusion.h   as they are
#   page.inc          mission control's page from diffusion.cpp, tools/pi-page.sed
#   control/page_texts.inc  mission control's texts, from the page's TX
#                     object (tools/page-texts.awk), for src/control/texts.c
# Stops if a change no longer applies (new version of the game) or if a
# displayed text names the Cardputer, or a Cardputer key on a gamepad.
#
#   GAME_DIR=... OUT=... tools/prepare.sh
set -e

HERE=$(cd "$(dirname "$0")/.." && pwd)
GAME=${GAME_DIR:-$HERE/deps/Escape-Game-Cardputer}
OUT=${OUT:-$HERE/build/gen}
T=$HERE/tools

fail() {
    echo "prepare.sh: $*" >&2
    exit 1
}

[ -f "$GAME/src/main.cpp" ] || fail "game not found in $GAME (GAME_DIR)"
mkdir -p "$OUT/control"

# Unchanged files keep their date: no rebuild
keep() {  # keep <file>: moves <file>.new over <file> if different
    if cmp -s "$1.new" "$1"; then rm "$1.new"; else mv "$1.new" "$1"; fi
}

# ---- main.cpp
sed -f "$T/pi-game.sed" "$GAME/src/main.cpp" | cat - "$HERE/src/bridge.inc" > "$OUT/main.cpp.new"
grep -q 'return padText(t, lang);' "$OUT/main.cpp.new" || fail "tools/pi-game.sed no longer applies (tr)"
grep -q '^const char \*padText(Tx &t, int l);' "$OUT/main.cpp.new" || fail "tools/pi-game.sed no longer applies (padText)"
grep -q 'return tr(multiMode ? multi : solo);' "$OUT/main.cpp.new" || fail "tools/pi-game.sed no longer applies (trm)"
keep "$OUT/main.cpp"

# ---- texts: keyboard, Xbox, PlayStation
sed -f "$T/pi-texts.sed" "$GAME/src/textes.h" > "$OUT/textes.h.new"
grep -q '"Ctrl Ctrl Ctrl : reprendre"' "$OUT/textes.h.new" || fail "tools/pi-texts.sed no longer applies (pause)"
pad() {  # pad <namespace> <sed buttons>
    sed -f "$T/pi-texts.sed" "$GAME/src/textes.h" | sed -f "$T/pad-texts.sed" | sed "$2" |
        sed "s/^namespace textes {/namespace $1 {/; s/^}  \/\/ namespace textes$/}  \/\/ namespace $1/"
}
#   style        confirm back keyboard left help pause
#   Xbox, 8BitDo A       B    Y        X    LB   Start
#   PlayStation  ×       ○    △        □    L1   Options
pad textes_xbox 's/@OK@/A/g; s/@BACK@/B/g; s/@KB@/Y/g; s/@SQ@/X/g; s/@L@/LB/g; s/@R@/Start/g' > "$OUT/textes_xbox.h.new"
pad textes_ps 's/@OK@/×/g; s/@BACK@/○/g; s/@KB@/△/g; s/@SQ@/□/g; s/@L@/L1/g; s/@R@/Options/g' > "$OUT/textes_ps.h.new"
grep -q '^namespace textes_xbox {' "$OUT/textes_xbox.h.new" || fail "textes.h: namespace not found"

# No displayed text may contain "Cardputer", nor, on a gamepad, a Cardputer
# key name or a placeholder left over (comments may)
quoted() {
    sed 's#//.*##' "$@" | grep '"' || true
}
if quoted "$OUT/textes.h.new" "$OUT/main.cpp" "$GAME/src/mots.h" | grep -i 'cardputer'; then
    fail "a displayed text names the Cardputer: extend tools/pi-texts.sed"
fi
if quoted "$OUT/textes_xbox.h.new" "$OUT/textes_ps.h.new" | grep -E 'ENTRÉE|ENTER|ESC|ESPACE|SPACE|TAB|DEL|Fn|Ctrl|@[A-Z]+@|"R : |R: refresh|A-Z|1-9'; then
    fail "a gamepad text names a key: extend tools/pad-texts.sed"
fi

# The texts side by side: every "constexpr Tx NAME" and "constexpr Tx NAME[n]"
awk '
/^constexpr Tx [A-Z0-9_]+\[/ {
    name = $3; sub(/\[.*/, "", name)
    printf "for (size_t i = 0; i < sizeof(textes::%s) / sizeof(textes::%s[0]); i++) add(textes::%s[i], textes_xbox::%s[i], textes_ps::%s[i]);\n", name, name, name, name, name
    next
}
/^constexpr Tx [A-Z0-9_]+ =/ {
    printf "add(textes::%s, textes_xbox::%s, textes_ps::%s);\n", $3, $3, $3
}' "$OUT/textes.h.new" > "$OUT/texts_table.inc.new"
[ "$(wc -l < "$OUT/texts_table.inc.new")" -gt 50 ] || fail "texts_table.inc: too few texts found in textes.h"
for f in textes.h textes_xbox.h textes_ps.h texts_table.inc; do
    keep "$OUT/$f"
done

# ---- game data and network header, as they are
for f in mots.h dessins.h diffusion.h; do
    cp "$GAME/src/$f" "$OUT/$f.new"
    keep "$OUT/$f"
done

# ---- mission control's page
awk '/const char PAGE\[\] PROGMEM = R"rawliteral\(/{f=1} f{print} /\)rawliteral";/{f=0}' "$GAME/src/diffusion.cpp" |
    sed -f "$T/pi-page.sed" > "$OUT/page.inc.new"
grep -q '^const char PAGE\[\] = R"rawliteral(' "$OUT/page.inc.new" || fail "tools/pi-page.sed no longer applies (PAGE)"
# (the page's other texts may say ENTRÉE: the maze's entrance)
if grep -E "[a-z0-9]+:(\"[^\"]*|'[^']*)Cardputer" "$OUT/page.inc.new" ||
    grep -E "(cx|r4):(\"[^\"]*|'[^']*)(ENTRÉE|ENTER)" "$OUT/page.inc.new"; then
    fail "tools/pi-page.sed no longer applies to the page of diffusion.cpp"
fi
keep "$OUT/page.inc"

# ---- mission control's texts: those of the page, in the order of
# src/control/texts.h
awk -f "$T/page-texts.awk" "$OUT/page.inc" > "$OUT/control/page_texts.inc.new" ||
    fail "tools/page-texts.awk: the page's TX object changed"
keep "$OUT/control/page_texts.inc"
echo "prepare.sh: sources ready in $OUT"
