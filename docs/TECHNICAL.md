# Explorer3-RaspberryPi: technical documentation

*Version française : [TECHNIQUE.md](TECHNIQUE.md).*

## Contents

1. [Goal and status](#1-goal-and-status)
2. [Decisions](#2-decisions)
3. [Build](#3-build)
4. [Code layout](#4-code-layout)
5. [The game on the Pi](#5-the-game-on-the-pi)
6. [Texts: keyboard, Xbox, PlayStation](#6-texts-keyboard-xbox-playstation)
7. [Keyboard, gamepads, on-screen keyboard, role choice](#7-keyboard-gamepads-on-screen-keyboard-role-choice)
8. [Network](#8-network)
9. [Multiplayer: the Crew](#9-multiplayer-the-crew)
10. [Multiplayer: Mission control](#10-multiplayer-mission-control)
11. [Display and sound](#11-display-and-sound)
12. [Package and start-up](#12-package-and-start-up)
13. [Tests](#13-tests)
14. [Next steps](#14-next-steps)

## 1. Goal and status

One program, `explorer3`, for the **Raspberry Pi 3** and **Pi 400** under Raspberry Pi OS, with all the game's features. The game is the Cardputer's ([Escape-Game-Cardputer](https://github.com/RyoSaeba89/Escape-Game-Cardputer) v3.0.0): `src/main.cpp` is **compiled for Linux**, as its PC simulator and [Explorer3-Android](https://github.com/RyoSaeba89/Explorer3-Android) already do. Mission control is in this repository (`src/control/`). This port does not modify the game: it is read from its folder, and what changes for the Pi is applied to copies at build time. Most of the Pi code comes from Explorer3-Android (game thread, memory panel, speaker, on-screen keyboard, role menu, crew server, mission control).

**Status (2026-10-13)**: code written, not yet compiled nor tried on a Pi.

## 2. Decisions

Made by the user on 2026-10-13. Not to be reopened without a new reason.

| Topic | Decision |
|---|---|
| Machines | Raspberry Pi 3 and Pi 400, Raspberry Pi OS **Bookworm and Trixie, 32-bit (armhf) and 64-bit (arm64)**. |
| Start | **Both**: an app of the desktop (menu, full screen) and, without a desktop (Raspberry Pi OS Lite), straight on the console (SDL's KMSDRM). |
| Start with the Pi | `sudo explorer3-autostart on` / `off`, **off by default**. |
| Input | **Keyboard** (the Cardputer's keys) **and gamepads** (Xbox, PlayStation, 8BitDo and generic ones), on-screen keyboard as on Android. |
| On-screen hints | **Those of the last device used**: keyboard keys, A B X Y (Xbox, 8BitDo), × ○ □ △ (PlayStation), switching by themselves. |
| Pause (Fn × 3) | **Ctrl × 3** on the keyboard (Fn cannot be read), **Start** on a gamepad. |
| Quit | **Ctrl+Q** (and Alt+F4 on the desktop), **Start + Select** together on a gamepad. |
| Word "Cardputer" | **Nowhere** in the texts, as on Android ("Cardputer seul" becomes "Solo"). |
| Roles | Solo, Crew and Mission control, as on Android. |
| Network | **Like the Cardputer**: Wi-Fi list and password in the game, Ethernet, **Explorer3 network with captive portal**. |
| Crew ports | **80 and 81**, as on the Cardputer: address without a port, `explorer3.local`. Granted at install time (`setcap`), never as root. |
| Install | **.deb** built by GitHub Actions for each version (arm64 and armhf), `sudo apt install ./explorer3_<v>_<arch>.deb`. |
| Publishing | Public repository **Explorer3-RaspberryPi**, MIT licence, documentation in French and English, code and comments in English; created and pushed only **after a test on a Pi**. |
| Building here | WSL2 with Debian on the user's PC. |

## 3. Build

`tools/fetch-deps.sh` puts into `deps/` the versions of `tools/versions.sh`:

- **Escape-Game-Cardputer** v3.0.0 (`src/`, and `sim/include`, the simulator's Arduino headers);
- **M5GFX** 0.2.32, as the Cardputer build and Explorer3-Android.

Local copies can be used instead: `make GAME_DIR=... GFX_DIR=.../src`.

Then, on the Pi or on a Debian PC with `build-essential`, `pkg-config` and `libsdl2-dev`:

```sh
make           # build/explorer3
make deb       # dist/explorer3_<version>_<arch>.deb
```

For the Pi from a Debian 12 PC: `dpkg --add-architecture arm64` (or `armhf`), `crossbuild-essential-arm64` and `libsdl2-dev:arm64`, then `make deb CROSS=aarch64-linux-gnu- PKG_CONFIG="env PKG_CONFIG_LIBDIR=/usr/lib/aarch64-linux-gnu/pkgconfig:/usr/share/pkgconfig pkg-config"`. That is what `.github/workflows/release.yml` does for each `v...` tag, in a `debian:bookworm` container: built against Debian 12, the program also runs on Debian 13.

`tools/prepare.sh` (run by `make`) makes the copies of section 5 in `build/gen`. It **stops** if one of its changes no longer applies (new version of the game), if a displayed text names the Cardputer, or if a gamepad text still names a keyboard key.

Build settings: M5GFX's `LGFX_LINUX_FB` platform (memory panel of `display_m5gfx.cpp`); `-ffunction-sections -fdata-sections` and `--gc-sections`, which drop the fonts the game does not use.

## 4. Code layout

| File | Role |
|---|---|
| `src/main.cpp` | Display thread: SDL window, keyboard, gamepads, sound, game or mission control |
| `src/game.cpp`, `game.h` | Game thread (`setup()` then `loop()`), Arduino functions, memory (`settings.txt`), published picture |
| `src/display_m5gfx.cpp`, `display.h` | In-memory screen (240 x 135), Android's |
| `src/include/M5Cardputer.h`, `Preferences.h` | Replace the simulator's: real speaker, memory in a file, the Pi's current network |
| `src/audio.cpp`, `audio.h` | The Cardputer's speaker (Android's mixer at 48 kHz) |
| `src/input.cpp`, `input.h` | Keys and buttons turned into Cardputer key presses |
| `src/labels.cpp`, `labels.h` | Last device used: keyboard, Xbox or PlayStation hints |
| `src/onscreen_keyboard.cpp` | On-screen keyboard (Android's, plus a text page) |
| `src/role_menu.cpp` | Role choice after "Multiplayer" |
| `src/bridge.h`, `bridge.inc` | Reads the game's state and picks the texts (appended to the copy of `main.cpp`) |
| `src/mirror_pi.cpp`, `sha1.h` | `mirror::`: the crew's server (page, stream, captive portal) |
| `src/netctl.cpp`, `netctl.h` | NetworkManager (nmcli): status, Wi-Fi list, joining, Explorer3 network, `explorer3.local` |
| `src/control/control.c` | Mission control: main loop (Android's without Java) |
| `src/control/session.c`, `ws.c`, `msg.c`, `rows.c`, `clocksync.c` | Protocol: WebSocket, messages, picture rows, the crew's clock |
| `src/control/panel.c`, `banner.c`, `sound.c` | Pages (rules, puzzles, the maze and its theme, coded keypad), end banner, sound |
| `src/control/draw.c`, `text.c`, `texts.c`, `font_8x16.c` | Drawing on the 960 x 540 canvas, texts (the page's), 8x16 font |
| `src/control/gamedata.h` | Morse code, astronaut and symbols, read from the game (`bridge.inc`) |
| `src/control/addr.c` | IPv4 addresses and address entry |
| `src/control/sdl_draw.c` | SDL_Renderer for `draw.h`, and the sharp copy of a 240 x 135 screen |
| `src/control/net.c` | Connection to the crew and search on the network (Android's) |
| `src/control/ui.c` | Mission control's screens before the stream (Android's, the Pi's keys) |
| `tools/prepare.sh`, `tools/*.sed`, `page-texts.awk` | The Pi's changes to the copies of the game and its page; mission control's texts taken from the page |
| `tools/fetch-deps.sh`, `versions.sh` | Sources needed, exact versions |
| `tools/make-deb.sh`, `packaging/` | The package: program, menu, icon, autostart, captive portal, polkit rule |

## 5. The game on the Pi

`main.cpp` is compiled with the headers of the game's PC simulator (`sim/include`), except `M5Cardputer.h` and `Preferences.h`, replaced by `src/include`. Copies made by `tools/prepare.sh`:

- `main.cpp`: `tools/pi-game.sed` (three lines: `tr()` and `trm()` go through `padText()`, section 6), then `src/bridge.inc` appended;
- `textes.h`, `textes_xbox.h`, `textes_ps.h` and `texts_table.inc` (section 6);
- `mots.h`, `dessins.h`, `diffusion.h` as they are;
- `page.inc`: mission control's page, taken from `diffusion.cpp`, `tools/pi-page.sed` (no "Cardputer", rule 4 without a key name).

As on Android: `M5Cardputer.Display` is a 240 x 135 memory panel (`offset_rotation = 3` cancels the game's `setRotation(1)`); right after each frame (`mirror::unlockScreen()`) the picture is copied, the role menu and the on-screen keyboard are drawn over it, and a picture number is advanced; the display thread shows it when the number has changed. The game runs on its own thread for as long as the program runs.

**Memory**: `~/.config/explorer3/settings.txt` (language, best time, Wi-Fi name and password, as in the Cardputer's NVS), readable by its owner only. A network the Pi is already on counts as the remembered one (`Preferences.h`): "Multiplayer" then goes straight to it, as the Cardputer does with its remembered Wi-Fi.

**Bridge** (`bridge.inc`): appended to the copy of `main.cpp`, it reads the game's internal variables (same translation unit) without changing anything: what the screen expects to be typed (including the Wi-Fi name and password), screen changes, the Wi-Fi list, "connect the screen", "Multiplayer" selected, the language.

## 6. Texts: keyboard, Xbox, PlayStation

The game's texts exist in three versions, all made from `src/textes.h`:

- **keyboard** (`textes`): `tools/pi-texts.sed` only. The Cardputer's key names stay (ENTRÉE, ESC, TAB, DEL, ESPACE: a Pi's keyboard has them); "Cardputer seul" becomes "Solo", "Tout le jeu sur cet appareil"; "Fn Fn Fn" becomes "Ctrl Ctrl Ctrl"; "CONNEXION AU WI-FI" becomes "CONNEXION AU RÉSEAU" (Ethernet too); "Mémoire insuffisante" becomes "Port 80 ou 81 déjà utilisé" (a Pi does not run out of memory there, but another program may hold the ports);
- **Xbox** (`textes_xbox`) and **PlayStation** (`textes_ps`): then `tools/pad-texts.sed` (Explorer3-Android's), whose placeholders become:

| Placeholder | Role | Xbox, 8BitDo | PlayStation |
|---|---|---|---|
| `@OK@` | confirm (ENTER) | A | × |
| `@BACK@` | back (ESC) | B | ○ |
| `@KB@` | on-screen keyboard | Y | △ |
| `@SQ@` | SPACE, erase | X | □ |
| `@L@` | help (TAB) | LB | L1 |
| `@R@` | pause (Fn × 3) | Start | Options |

`texts_table.inc` lists each text of the three versions side by side (`add(textes::X, textes_xbox::X, textes_ps::X)`, a loop for the arrays). In the copy of `main.cpp`, `tr()` calls `padText()` (`bridge.inc`), which finds the text in that table by its address and returns the version of `labels::style()`, the last device used. The game itself does not know.

## 7. Keyboard, gamepads, on-screen keyboard, role choice

**Keyboard** (as in the PC simulator): letters, digits and punctuation in the keyboard's own layout (SDL text input), Enter, Esc (the Cardputer's `` ` ``), Backspace and Delete (DEL), Tab, Space, arrows (the Cardputer's `;` `.` `,` `/`; ignored where a name or password is typed), **Ctrl = Fn** (three times: pause). No key repeat, like the Cardputer's keyboard. A `` ` `` typed as a character is passed with Shift, so a password may hold one (the game only goes back on an unshifted `` ` ``).

**Gamepads** (SDL_GameController, buttons by position whatever the labels):

| Role | Cardputer key | Xbox, 8BitDo | PlayStation |
|---|---|---|---|
| Move | `;` `.` `,` `/` | D-pad, left stick | D-pad, left stick |
| Confirm | ENTER | A | × |
| Back | ESC | B | ○ |
| On-screen keyboard | — | Y | △ |
| Morse, erase, refresh (Wi-Fi list) | SPACE, DEL, R | X | □ |
| Help | TAB | LB, Select (released) | L1, Share (released) |
| Pause | Fn × 3 | RB, Start (released) | R1, Options (released) |
| Quit | — | Start + Select | Options + Share |

The style (Xbox or PlayStation) comes from `SDL_GameControllerGetType()`; every key or button sets `labels::style()`.

**On-screen keyboard** (`onscreen_keyboard.cpp`): Android's (Morse letters, quiz letters, start-up code, coded keypad), plus a **text** page for the Wi-Fi name and password: three rows of characters (a–z, A–Z, or symbols), then ABC (capitals), #+= (symbols), space, ← (erase) and OK. With a gamepad it opens by itself for the code, the coded keypad and the Wi-Fi typing; it closes as soon as the keyboard is used.

**Role choice** (`role_menu.cpp`, Android's, keys of the device in use): confirming "Multiplayer" opens "Crew" / "Mission control". Either way the game goes on into its network screens (section 8). As mission control, `startServer()` starts nothing, and when the game reaches "Connect the screen" (the network is up), `input.cpp` asks for the control screen; leaving it brings the game back to its mode menu (two Esc).

## 8. Network

`netctl.cpp` drives **NetworkManager** through `nmcli` (Raspberry Pi OS's network since Bookworm), on threads of its own, without a shell (names and passwords are plain arguments), messages in English (`LC_ALL=C`) to read the causes:

- **status** every 2 s (1 s with the Explorer3 network): `nmcli device`; Explorer3 network first, then Ethernet ("Ethernet"), then Wi-Fi (connection name); address from `getifaddrs()`;
- **Wi-Fi list** (`mirror::startScan`/`pollScan`, the Cardputer's two passes): first the networks NetworkManager already knows (`--rescan no`), then a new search (`--rescan yes`); signal 0..100 turned into dBm for the game's bars; the Explorer3 network is taken down first;
- **joining** (`mirror::connect`): already on it: done at once; otherwise `nmcli device wifi connect <ssid> password <pass>`, retried with `hidden yes` for "Other network"; "Secrets were required" → wrong password (the game says so at once), "No network with SSID" → not found; a profile made by a failed attempt is deleted;
- **Explorer3 network**: a connection kept in memory only (`save no`), access point, WPA2 (CCMP), `Explorer3` / `Explorer3`, **192.168.4.1/24** as on the Cardputer, `ipv4.method shared`: NetworkManager's dnsmasq hands out addresses, and `packaging/dnsmasq-explorer3.conf` makes it answer every name with 192.168.4.1 (captive portal, section 9). Setting it up takes a few seconds and blocks the game thread: the display thread says so meanwhile ("Création du réseau Explorer3…"). Devices joined: `iw dev <wlan> station dump`;
- **explorer3.local**: `avahi-publish -a -R explorer3.local <address>` while the crew's server runs, again when the address changes.

Rights: `packaging/50-explorer3.rules` (polkit) lets members of the `netdev` group (the Pi's first user is one) use NetworkManager without a password, also from the console.

## 9. Multiplayer: the Crew

`src/mirror_pi.cpp` is `diffusion.cpp`'s server in POSIX sockets (Explorer3-Android's, on the Cardputer's ports):

- `http://<address>/` (port **80**): the page (`page.inc`) and `/sym.js`, each request on a short-lived thread (8 at most);
- `ws://<address>:81/`: WebSocket handshake, compressed screen rows (25 pictures/s at most, only changed rows, the whole screen on each new connection except the banner), messages `P` (clock, 250 ms), `T` `S` `R` (sound), `K` (pages), `L` (language), `B` (banner), `M` (maze theme); ping every 5 s, disconnection after 2 unanswered; 5 browsers at most;
- **captive portal** on the Explorer3 network: a request whose `Host` is neither 192.168.4.1 nor `explorer3.local` gets a 302 to `http://192.168.4.1/` (`/sym.js` is always served, mission control's search needs it).

The ports are opened by the network thread at the game's request (`startServer`), both or none: if another program holds 80 or 81, the game shows "Port 80 ou 81 déjà utilisé" on its Wi-Fi list. Closed when this Pi becomes mission control.

## 10. Multiplayer: Mission control

All of it is in `src/control/`, pure C except `sdl_draw.c`, `net.c` and `control.c`. It does what the game's web page does, with the same messages (section 9):

- **Protocol**: WebSocket handshake and frames (`ws.c`), text messages (`msg.c`), picture rows (`rows.c`), the crew's clock (`clocksync.c`, the smallest difference of the last 40 messages, as the page).
- **Pages** (`panel.c`): rules, the 4 puzzles, coded keypad, O2 countdown turning red under a minute, red frame at each error. The **maze** shows the planets or, with message `M`, the game's theme: 16 x 16 pictures and their palette, the cells' names, the `p4t` text and the theme's hint. Morse code, astronaut and the keypad's symbols are read from the game compiled with it (`gamedata.h`, `bridge.inc`): always those of its version.
- **Texts**: those of the page's `TX` object, word for word, taken from `page.inc` by `tools/page-texts.awk` (`build/gen/control/page_texts.inc`); `prepare.sh` stops if one is missing. The texts of the screens before the stream are in `ui.c`.
- End screen **banner** (`banner.c`) and **sound** (`sound.c`: notes, stops, take-off rumble, as Web Audio in the page).
- **Font**: `font_8x16.c`, the 8x16 font (code page 437) of [libogc2](https://github.com/extremscorner/libogc2), copied as is with its zlib licence; `text.c` converts the UTF-8.


- **Search** (`net.c`, Android's, own thread): the 254 addresses of the Pi's /24, ports **81** (Cardputer, Pi) and **8081** (Android crew), checked with `GET /sym.js` on 80 or 8080. One crew found: connects at once; several: list; none: search again or type the address.
- **Address entry** (`addr.c`): arrows, ±1; L / R or - / +, ±10.
- **Stream**: screen copy and pages; reconnects by itself; Back goes back to the search, then to the game's menu.
- **Keys** drawn as the device in use: keyboard keys (Enter, Échap, Tab, - +), Xbox A B Y LB RB, PlayStation symbols L1 R1.
- Settings: `~/.config/explorer3/control.conf` (lines `ip=`: last address that answered, `lang=`: language).

## 11. Display and sound

**Display** (`main.cpp`, `control/sdl_draw.c`): one SDL window, `SDL_WINDOW_FULLSCREEN_DESKTOP` (`--window` for tests), SDL_Renderer (OpenGL ES on the Pi; KMSDRM without a desktop), vsync. The game's picture, like the crew's screen copy, is enlarged a whole number of times k **without smoothing** into a target texture, then brought to its final size (the largest 16:9 that fits, centred) **with smoothing**: only the edges of the big pixels soften (Android's method). Mission control draws a 960 x 540 canvas scaled to fit: rectangles, and the 8x16 font enlarged 4 times then smoothed. The mouse pointer is hidden in full screen.

**Sound**: one SDL output, 48 kHz stereo, 1024-frame blocks. The game's speaker (`audio.cpp`, Android's mixer: 8 channels, sine notes, looped raw samples) or, as mission control, the synthesis of `control/sound.c`, anchored at "now + 2 blocks". The latter is rendered all the time so that its clock stays right. In multiplayer the crew's game sends its sounds to mission control, as on the Cardputer.

## 12. Package and start-up

`tools/make-deb.sh` (`make deb`):

| Path | Content |
|---|---|
| `/usr/bin/explorer3` | the program; `postinst`: `setcap cap_net_bind_service=+ep` (ports 80 and 81) |
| `/usr/bin/explorer3-autostart` | `on`, `off`, `status` |
| `/usr/share/applications/explorer3.desktop`, icon | the *Games* menu entry |
| `/etc/NetworkManager/dnsmasq-shared.d/explorer3.conf` | captive portal (configuration file) |
| `/usr/share/polkit-1/rules.d/50-explorer3.rules` | NetworkManager without a password for `netdev` |

Depends: SDL2, NetworkManager, dnsmasq-base, avahi-utils, iw, libcap2-bin.

**Beware**: the dnsmasq file applies to every *shared* connection of NetworkManager, not only to the Explorer3 network: another access point made on the same Pi would also answer 192.168.4.1 to every name.

`explorer3-autostart on` (with sudo, for the user who runs it): if the default target is graphical, a copy of the menu entry in that user's `~/.config/autostart` (Raspberry Pi OS's desktop starts it); otherwise `/etc/systemd/system/explorer3.service`: the game on tty1 as that user (`PAMName=login`, so a real session: screen, sound, input), in place of `getty@tty1`, which is started again when the game is quit.

## 13. Tests

None yet. To do: build in WSL (amd64 version in a window, then the `.deb`), then on a Pi 3 and a Pi 400: Solo with the keyboard and with each gamepad; Crew with a browser, an Android device; Mission control facing a Cardputer, an Android device; Explorer3 network with a phone (captive portal); Ethernet; desktop and Lite; `explorer3-autostart` both ways.

## 14. Next steps

- First build and tests (section 13).
