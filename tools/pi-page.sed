# Mission control's page, copied from the Cardputer's diffusion.cpp into
# page.inc by tools/prepare.sh, changed for the Pi only:
s/ PROGMEM//
# No "Cardputer" in any displayed text (user's rule)
s/Connexion au Cardputer…/Connexion à l’équipage…/
s/Connecting to the Cardputer…/Connecting to the crew…/
# Rule 4: the crew may play on the keyboard or a gamepad
s/Le chrono démarre quand l'équipage appuie sur ENTRÉE\./Le chrono démarre quand l'équipage lance la partie./
s/The countdown starts when the crew presses ENTER\./The countdown starts when the crew starts the game./
