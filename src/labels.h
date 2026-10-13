// Which keys or buttons the on-screen hints name (user's choice): those of
// the last device used. The keyboard shows the Cardputer's key names
// (ENTRÉE, ESC, TAB...), an Xbox-style gamepad A B X Y, a PlayStation one
// × ○ □ △. The game's texts exist in the three versions (tools/prepare.sh);
// bridge.inc picks the right one each time the game asks for a text.
#pragma once

namespace labels {

enum Style { KEYBOARD = 0, XBOX = 1, PLAYSTATION = 2 };

Style style();  // any thread
void setStyle(Style s);

// Names of the confirm and back keys, for the screens drawn outside the game
// (role menu): lang 0 French, 1 English
const char *ok(int lang);
const char *back(int lang);

}  // namespace labels
