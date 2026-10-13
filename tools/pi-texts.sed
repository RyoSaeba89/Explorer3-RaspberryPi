# Game texts changed for the Pi, whatever the device (keyboard or gamepad).
# tools/prepare.sh applies this file to a copy of src/textes.h (the Cardputer
# project itself is never modified). The user's rule: the word "Cardputer"
# appears nowhere, in French or English (prepare.sh stops otherwise).
/^constexpr Tx MODE_SOLO = /c\
constexpr Tx MODE_SOLO = {"Solo", "Solo"};
/^constexpr Tx MODE_SOLO_INFO = /c\
constexpr Tx MODE_SOLO_INFO = {"Tout le jeu sur cet appareil", "Everything on this device"};
# Ethernet or Wi-Fi
/^constexpr Tx CONNECT_TITLE = /c\
constexpr Tx CONNECT_TITLE = {"CONNEXION AU RÉSEAU", "CONNECTING TO THE NETWORK"};
# The page server's ports may be taken by another program (no memory shortage on a Pi)
/^constexpr Tx ERR_MEMORY = /c\
constexpr Tx ERR_MEMORY = {"Port 80 ou 81 déjà utilisé", "Port 80 or 81 already in use"};
# The game master's pause: Ctrl 3 times (user's choice; Fn cannot be read)
s/"Fn Fn Fn : reprendre", "Fn Fn Fn: resume"/"Ctrl Ctrl Ctrl : reprendre", "Ctrl Ctrl Ctrl: resume"/
