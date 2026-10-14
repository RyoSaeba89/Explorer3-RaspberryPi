# Explorer3-RaspberryPi

**FR** — Le jeu [Escape Game Explorer 3](https://github.com/RyoSaeba89/Escape-Game-Cardputer) sur **Raspberry Pi 3** et **Raspberry Pi 400**, avec toutes ses fonctions : mode **Solo**, et **Multijoueur** où le Pi est soit l'**Équipage** (il joue ; un navigateur ou un autre appareil sert de centre de contrôle), soit le **Centre de contrôle** (il sert d'écran face à un Cardputer, un appareil Android ou un autre Pi). Au clavier ou à la manette, en français ou en anglais.

**EN** — The game [Escape Game Explorer 3](https://github.com/RyoSaeba89/Escape-Game-Cardputer) on the **Raspberry Pi 3** and **Raspberry Pi 400**, with all its features: **Solo** mode, and **Multiplayer** where the Pi is either the **Crew** (it plays; a web browser or another device is mission control) or **Mission control** (it is the screen, facing a Cardputer, an Android device or another Pi). Keyboard or gamepad, French or English.

> **Version 4.0.0 (2026-10-14)** — pour le jeu v4.0.0, le même numéro sur le Cardputer, Android et le Raspberry Pi, qui jouent ensemble. Le jeu s'ouvre dans une fenêtre 1280 × 720 ; F11 passe en plein écran et retour, et le choix est gardé. Le choix du rôle est celui du jeu. Centre de contrôle : une adresse tapée pendant la recherche est prise tout de suite ; « réessayer » retrouve un équipage Android. Équipage : une connexion muette ne garde plus une place. / for game v4.0.0, the same number on the Cardputer, Android and the Raspberry Pi, which play together. The game opens in a 1280 × 720 window; F11 switches to full screen and back, and the choice is kept. The role choice is the game's own. Mission control: an address typed during the search is taken at once; "try again" finds an Android crew. Crew: a silent connection no longer keeps a place.
>
> **Version 3.0.0 (2026-10-13)** — pour le jeu v3.0.0 ; essayée sur un Pi 400. / for game v3.0.0; tried on a Pi 400.

## Installation

**FR**
1. Raspberry Pi OS (Bookworm ou Trixie, 32 ou 64 bits), avec ou sans bureau. Pour le Wi-Fi, le pays doit être réglé (`sudo raspi-config`, *Localisation Options*, *WLAN Country*).
2. Télécharger dans la page [Releases](../../releases) le `.deb` qui correspond au système : `arm64` pour le 64 bits, `armhf` pour le 32 bits (`dpkg --print-architecture` le dit).
3. L'installer : `sudo apt install ./explorer3_<version>_<arch>.deb`
4. Lancer **Explorer 3** depuis le menu *Jeux* du bureau, ou taper `explorer3` (aussi sans bureau, depuis la console, alors toujours en plein écran). Sur le bureau, le jeu s'ouvre dans une fenêtre ; **F11** passe en plein écran et retour, et le jeu s'en souvient. `explorer3 --window` ou `--fullscreen` imposent l'un ou l'autre.
5. Facultatif : démarrer le jeu à l'allumage du Pi, `sudo explorer3-autostart on` (`off` pour arrêter). Avec le bureau, le jeu s'ouvre avec la session ; sans bureau, il remplace l'invite de connexion de la première console, qui revient quand on quitte le jeu.

**EN**
1. Raspberry Pi OS (Bookworm or Trixie, 32 or 64-bit), with or without a desktop. For the Wi-Fi, the country must be set (`sudo raspi-config`, *Localisation Options*, *WLAN Country*).
2. Download from the [Releases](../../releases) page the `.deb` matching the system: `arm64` for 64-bit, `armhf` for 32-bit (`dpkg --print-architecture` tells).
3. Install it: `sudo apt install ./explorer3_<version>_<arch>.deb`
4. Start **Explorer 3** from the desktop's *Games* menu, or type `explorer3` (also without a desktop, from the console, then always full screen). On the desktop the game opens in a window; **F11** switches to full screen and back, and the game remembers it. `explorer3 --window` or `--fullscreen` force one or the other.
5. Optional: start the game when the Pi is switched on, `sudo explorer3-autostart on` (`off` to stop). With the desktop, the game opens with the session; without, it replaces the first console's login prompt, which comes back when the game is quit.

## Commandes / Controls

**FR** — Les aides à l'écran suivent ce qu'on vient d'utiliser : les touches du clavier, ou les boutons de la manette (A B X Y pour une manette Xbox ou 8BitDo, × ○ □ △ pour une PlayStation).

**EN** — The on-screen hints follow what was used last: the keyboard's keys, or the gamepad's buttons (A B X Y for an Xbox or 8BitDo pad, × ○ □ △ for a PlayStation one).

| Rôle / Role | Clavier / Keyboard | Xbox, 8BitDo | PlayStation |
|---|---|---|---|
| Déplacer / Move | Flèches / Arrows | croix, stick gauche / D-pad, left stick | croix, stick gauche / D-pad, left stick |
| Valider / Confirm | Entrée / Enter | A | × |
| Retour / Back | Échap / Esc | B | ○ |
| Répondre (lettres, chiffres) / Answer (letters, digits) | les touches / the keys | Y : clavier à l'écran / on-screen keyboard | △ : clavier à l'écran / on-screen keyboard |
| Revoir, réécouter le Morse / Watch, listen again | Espace / Space | X | □ |
| Effacer / Erase | Retour arrière, Suppr / Backspace, Delete | X | □ |
| Alphabet Morse / Morse code | Tab | LB, Select | L1, Share |
| Pause (maître du jeu / game master) | Ctrl × 3 | Start, RB | Options, R1 |
| Quitter / Quit | Ctrl+Q (Alt+F4 sur le bureau / on the desktop) | Start + Select | Options + Share |

**FR** — À la manette, le clavier à l'écran s'ouvre tout seul là où il faut taper un code ou un mot de passe Wi-Fi (ABC : majuscules, #+= : symboles).

**EN** — With a gamepad, the on-screen keyboard opens by itself wherever a code or a Wi-Fi password is typed (ABC: capitals, #+=: symbols).

## Multijoueur / Multiplayer

**FR**
- Après **Multijoueur**, choisir **Équipage** ou **Centre de contrôle**, puis le réseau, comme sur le Cardputer : le réseau où le Pi est déjà (câble Ethernet, ou Wi-Fi réglé dans Raspberry Pi OS) est pris tout de suite ; sinon la liste des Wi-Fi, avec **Créer le réseau Explorer3** (`Explorer3`, mot de passe `Explorer3` ; un téléphone qui le rejoint ouvre la page tout seul).
- **Équipage** : le Pi affiche un QR code, son adresse (`http://192.168.1.42`) et `explorer3.local`, comme le Cardputer. La page s'ouvre dans un navigateur ; un appareil Android ([Explorer3-Android](https://github.com/RyoSaeba89/Explorer3-Android)) ou un autre Pi peuvent aussi servir de centre de contrôle.
- **Centre de contrôle** : le Pi cherche l'équipage tout seul sur le réseau (Cardputer, Android ou Pi) ; sinon Tab / Y / △ pour taper son adresse, même pendant la recherche.

**EN**
- After **Multiplayer**, choose **Crew** or **Mission control**, then the network, as on the Cardputer: the network the Pi is already on (Ethernet cable, or a Wi-Fi set up in Raspberry Pi OS) is used at once; otherwise the Wi-Fi list, with **Create the Explorer3 network** (`Explorer3`, password `Explorer3`; a phone joining it opens the page by itself).
- **Crew**: the Pi shows a QR code, its address (`http://192.168.1.42`) and `explorer3.local`, as the Cardputer does. The page opens in a web browser; an Android device ([Explorer3-Android](https://github.com/RyoSaeba89/Explorer3-Android)) or another Pi can also be mission control.
- **Mission control**: the Pi looks for the crew on the network by itself (Cardputer, Android or Pi); otherwise Tab / Y / triangle to type its address, even during the search.

## Documentation

| | |
|---|---|
| Documentation technique (français) | [docs/TECHNIQUE.md](docs/TECHNIQUE.md) |
| Technical documentation (English) | [docs/TECHNICAL.md](docs/TECHNICAL.md) |

**FR** — Pour compiler : sur le Pi ou sur un PC Debian, `sh tools/fetch-deps.sh` puis `make` (`make deb` pour le paquet) ; voir la documentation technique.

**EN** — To build: on the Pi or on a Debian PC, `sh tools/fetch-deps.sh` then `make` (`make deb` for the package); see the technical documentation.

## Licence / License

[MIT](LICENSE). **FR** — La police 8x16 du centre de contrôle vient de [libogc2](https://github.com/extremscorner/libogc2) (licence zlib, dans `src/control/font_8x16.c`). **EN** — Mission control's 8x16 font comes from [libogc2](https://github.com/extremscorner/libogc2) (zlib licence, in `src/control/font_8x16.c`).
