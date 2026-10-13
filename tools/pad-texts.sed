# Game texts for the gamepad (Explorer3-Android's): tools/prepare.sh applies
# this file, after pi-texts.sed, to two more copies of src/textes.h, then
# turns the button placeholders into an Xbox pad's or a PlayStation pad's
# buttons (the keyboard copy keeps the Cardputer's key names):
#   @OK@ confirm, @BACK@ back, @KB@ on-screen keyboard, @SQ@ left face button,
#   @L@ help, @R@ pause
# prepare.sh stops if a quoted text still names a Cardputer key or keeps a
# placeholder (text added or changed in a newer version of the game).

# ---- Whole texts rewritten for the gamepad (before the general replacements)
/^constexpr Tx WIFI_REFRESH = /c\
constexpr Tx WIFI_REFRESH = {"@SQ@ : actualiser", "@SQ@: refresh"};
/^constexpr Tx WIFI_SCAN_FAILED = /c\
constexpr Tx WIFI_SCAN_FAILED = {"Recherche impossible : @SQ@ pour réessayer", "Search failed: @SQ@ to try again"};
/^constexpr Tx P1_FOOTER = /c\
constexpr Tx P1_FOOTER = {"@KB@ : répondre|@SQ@ : revoir|@L@ : Morse", "@KB@: answer|@SQ@: watch again|@L@: Morse"};
/^constexpr Tx P1_FOOTER_MULTI = /c\
constexpr Tx P1_FOOTER_MULTI = {"@KB@ : répondre|@SQ@ : revoir", "@KB@: answer|@SQ@: watch again"};
/^constexpr Tx P2_FOOTER = /c\
constexpr Tx P2_FOOTER = {"@KB@ : répondre", "@KB@: answer"};
/^constexpr Tx P3_FOOTER_MULTI = /c\
constexpr Tx P3_FOOTER_MULTI = {"@KB@ : répondre|@SQ@ : réécouter (contrôle)", "@KB@: answer|@SQ@: replay (mission control)"};
/^constexpr Tx P4_FOOTER = /c\
constexpr Tx P4_FOOTER = {"@KB@ : répondre|@SQ@ : réécouter|@L@ : Morse", "@KB@: answer|@SQ@: listen again|@L@: Morse"};
/^constexpr Tx MORSE_CLOSE = /c\
constexpr Tx MORSE_CLOSE = {"Un bouton : fermer", "Any button: close"};
/^constexpr Tx CODE_FOOTER = /c\
constexpr Tx CODE_FOOTER = {"@OK@ : taper|@SQ@ : effacer|@KB@ : clavier", "@OK@: type|@SQ@: erase|@KB@: keyboard"};
/^constexpr Tx KEYPAD_FOOTER = /c\
constexpr Tx KEYPAD_FOOTER = {"@OK@ : taper|@SQ@ : effacer|@KB@ : clavier", "@OK@: type|@SQ@: erase|@KB@: keyboard"};
/^constexpr Tx ADDR_FOOTER = /c\
constexpr Tx ADDR_FOOTER = {"@BACK@ : retour", "@BACK@: back"};
/^constexpr Tx PAUSE_RESUME = /c\
constexpr Tx PAUSE_RESUME = {"@R@ : reprendre", "@R@: resume"};

# ---- Cardputer key names in the other texts (only lines holding text)
/"/{
s/ENTRÉE \/ ENTER/@OK@/g
s/ENTRÉE/@OK@/g
s/ENTER/@OK@/g
s/ESPACE/@SQ@/g
s/SPACE/@SQ@/g
s/ESC/@BACK@/g
s/TAB/@L@/g
s/DEL/@SQ@/g
}
