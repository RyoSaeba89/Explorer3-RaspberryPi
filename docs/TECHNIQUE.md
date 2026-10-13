# Explorer3-RaspberryPi : documentation technique

*English version: [TECHNICAL.md](TECHNICAL.md).*

## Sommaire

1. [But et état](#1-but-et-état)
2. [Décisions](#2-décisions)
3. [Compilation](#3-compilation)
4. [Organisation du code](#4-organisation-du-code)
5. [Le jeu sur le Pi](#5-le-jeu-sur-le-pi)
6. [Textes : clavier, Xbox, PlayStation](#6-textes--clavier-xbox-playstation)
7. [Clavier, manettes, clavier à l'écran, choix du rôle](#7-clavier-manettes-clavier-à-lécran-choix-du-rôle)
8. [Réseau](#8-réseau)
9. [Multijoueur : l'Équipage](#9-multijoueur--léquipage)
10. [Multijoueur : le Centre de contrôle](#10-multijoueur--le-centre-de-contrôle)
11. [Affichage et son](#11-affichage-et-son)
12. [Paquet et démarrage](#12-paquet-et-démarrage)
13. [Essais](#13-essais)
14. [Suite](#14-suite)

## 1. But et état

Un seul programme, `explorer3`, pour le **Raspberry Pi 3** et le **Pi 400** sous Raspberry Pi OS, avec toutes les fonctions du jeu. Le jeu est celui du Cardputer ([Escape-Game-Cardputer](https://github.com/RyoSaeba89/Escape-Game-Cardputer) v3.0.0) : `src/main.cpp` est **compilé pour Linux**, comme le font déjà son simulateur PC et [Explorer3-Android](https://github.com/RyoSaeba89/Explorer3-Android). Le centre de contrôle est dans ce dépôt (`src/control/`). Ce portage ne modifie pas le jeu : il est lu dans son dossier, et ce qui change pour le Pi est appliqué à des copies au moment de compiler. L'essentiel du code Pi vient d'Explorer3-Android (fil du jeu, écran en mémoire, haut-parleur, clavier à l'écran, menu des rôles, serveur de l'équipage, centre de contrôle).

**État (2026-10-13)** : code écrit, pas encore compilé ni essayé sur un Pi.

## 2. Décisions

Prises par l'utilisateur le 2026-10-13. À ne pas rouvrir sans raison nouvelle.

| Sujet | Décision |
|---|---|
| Machines | Raspberry Pi 3 et Pi 400, Raspberry Pi OS **Bookworm et Trixie, 32 bits (armhf) et 64 bits (arm64)**. |
| Lancement | **Les deux** : une application du bureau (menu, plein écran) et, sans bureau (Raspberry Pi OS Lite), directement sur la console (KMSDRM de SDL). |
| Démarrage avec le Pi | `sudo explorer3-autostart on` / `off`, **désactivé par défaut**. |
| Commandes | **Clavier** (les touches du Cardputer) **et manettes** (Xbox, PlayStation, 8BitDo et génériques), clavier à l'écran comme sur Android. |
| Aides à l'écran | **Celles du dernier appareil utilisé** : touches du clavier, A B X Y (Xbox, 8BitDo), × ○ □ △ (PlayStation), qui changent toutes seules. |
| Pause (Fn × 3) | **Ctrl × 3** au clavier (Fn n'est pas lisible), **Start** à la manette. |
| Quitter | **Ctrl+Q** (et Alt+F4 sur le bureau), **Start + Select** ensemble à la manette. |
| Mot « Cardputer » | **Nulle part** dans les textes, comme sur Android (« Cardputer seul » devient « Solo »). |
| Rôles | Solo, Équipage et Centre de contrôle, comme sur Android. |
| Réseau | **Comme le Cardputer** : liste des Wi-Fi et mot de passe dans le jeu, Ethernet, **réseau Explorer3 avec portail captif**. |
| Ports de l'équipage | **80 et 81**, comme le Cardputer : adresse sans port, `explorer3.local`. Accordés à l'installation (`setcap`), jamais en root. |
| Installation | **.deb** compilé par GitHub Actions à chaque version (arm64 et armhf), `sudo apt install ./explorer3_<v>_<arch>.deb`. |
| Publication | Dépôt public **Explorer3-RaspberryPi**, licence MIT, documentation en français et en anglais, code et commentaires en anglais ; créé et poussé seulement **après un essai sur un Pi**. |
| Compilation ici | WSL2 avec Debian sur le PC de l'utilisateur. |

## 3. Compilation

`tools/fetch-deps.sh` met dans `deps/` les versions de `tools/versions.sh` :

- **Escape-Game-Cardputer** v3.0.0 (`src/`, et `sim/include`, les en-têtes Arduino du simulateur) ;
- **M5GFX** 0.2.32, comme le firmware du Cardputer et Explorer3-Android.

Des copies locales peuvent servir à la place : `make GAME_DIR=... GFX_DIR=.../src`.

Puis, sur le Pi ou sur un PC Debian avec `build-essential`, `pkg-config` et `libsdl2-dev` :

```sh
make           # build/explorer3
make deb       # dist/explorer3_<version>_<arch>.deb
```

Pour le Pi depuis un PC Debian 12 : `dpkg --add-architecture arm64` (ou `armhf`), `crossbuild-essential-arm64` et `libsdl2-dev:arm64`, puis `make deb CROSS=aarch64-linux-gnu- PKG_CONFIG="env PKG_CONFIG_LIBDIR=/usr/lib/aarch64-linux-gnu/pkgconfig:/usr/share/pkgconfig pkg-config"`. C'est ce que fait `.github/workflows/release.yml` à chaque tag `v...`, dans un conteneur `debian:bookworm` : compilé pour Debian 12, le programme tourne aussi sur Debian 13.

`tools/prepare.sh` (lancé par `make`) fait les copies de la section 5 dans `build/gen`. Il **s'arrête** si l'une de ses modifications ne s'applique plus (nouvelle version du jeu), si un texte affiché nomme le Cardputer, ou si un texte pour la manette nomme encore une touche du clavier.

Réglages : plateforme `LGFX_LINUX_FB` de M5GFX (l'écran en mémoire de `display_m5gfx.cpp`) ; `-ffunction-sections -fdata-sections` et `--gc-sections`, qui retirent les polices que le jeu n'utilise pas.

## 4. Organisation du code

| Fichier | Rôle |
|---|---|
| `src/main.cpp` | Fil d'affichage : fenêtre SDL, clavier, manettes, son, jeu ou centre de contrôle |
| `src/game.cpp`, `game.h` | Fil du jeu (`setup()` puis `loop()`), fonctions Arduino, mémoire (`settings.txt`), image publiée |
| `src/display_m5gfx.cpp`, `display.h` | Écran en mémoire (240 x 135), celui d'Android |
| `src/include/M5Cardputer.h`, `Preferences.h` | Remplacent ceux du simulateur : vrai haut-parleur, mémoire dans un fichier, réseau actuel du Pi |
| `src/audio.cpp`, `audio.h` | Le haut-parleur du Cardputer (mélangeur d'Android, à 48 kHz) |
| `src/input.cpp`, `input.h` | Touches et boutons changés en appuis du clavier du Cardputer |
| `src/labels.cpp`, `labels.h` | Dernier appareil utilisé : aides clavier, Xbox ou PlayStation |
| `src/onscreen_keyboard.cpp` | Clavier à l'écran (celui d'Android, plus une page de texte) |
| `src/role_menu.cpp` | Choix du rôle après « Multijoueur » |
| `src/bridge.h`, `bridge.inc` | Lit l'état du jeu et choisit les textes (ajouté à la copie de `main.cpp`) |
| `src/mirror_pi.cpp`, `sha1.h` | `mirror::` : le serveur de l'équipage (page, flux, portail captif) |
| `src/netctl.cpp`, `netctl.h` | NetworkManager (nmcli) : état, liste des Wi-Fi, connexion, réseau Explorer3, `explorer3.local` |
| `src/control/control.c` | Centre de contrôle : boucle principale (celle d'Android sans Java) |
| `src/control/session.c`, `ws.c`, `msg.c`, `rows.c`, `clocksync.c` | Protocole : WebSocket, messages, lignes d'image, horloge de l'équipage |
| `src/control/panel.c`, `banner.c`, `sound.c` | Pages (règles, énigmes, labyrinthe et son thème, clavier codé), bandeau de fin, son |
| `src/control/draw.c`, `text.c`, `texts.c`, `font_8x16.c` | Dessin sur la toile 960 x 540, textes (ceux de la page), police 8x16 |
| `src/control/gamedata.h` | Morse, astronaute et symboles, lus dans le jeu (`bridge.inc`) |
| `src/control/addr.c` | Adresses IPv4 et saisie de l'adresse |
| `src/control/sdl_draw.c` | SDL_Renderer pour `draw.h`, et la copie nette d'un écran 240 x 135 |
| `src/control/net.c` | Connexion à l'équipage et recherche sur le réseau (celles d'Android) |
| `src/control/ui.c` | Écrans du centre de contrôle avant le flux (ceux d'Android, touches du Pi) |
| `tools/prepare.sh`, `tools/*.sed`, `page-texts.awk` | Modifications pour le Pi des copies du jeu et de sa page ; textes du centre de contrôle tirés de la page |
| `tools/fetch-deps.sh`, `versions.sh` | Sources nécessaires, versions exactes |
| `tools/make-deb.sh`, `packaging/` | Le paquet : programme, menu, icône, démarrage automatique, portail captif, règle polkit |

## 5. Le jeu sur le Pi

`main.cpp` est compilé avec les en-têtes du simulateur PC du jeu (`sim/include`), sauf `M5Cardputer.h` et `Preferences.h`, remplacés par ceux de `src/include`. Copies faites par `tools/prepare.sh` :

- `main.cpp` : `tools/pi-game.sed` (trois lignes : `tr()` et `trm()` passent par `padText()`, section 6), puis `src/bridge.inc` ajouté à la fin ;
- `textes.h`, `textes_xbox.h`, `textes_ps.h` et `texts_table.inc` (section 6) ;
- `mots.h`, `dessins.h`, `diffusion.h` tels quels ;
- `page.inc` : la page du centre de contrôle, prise dans `diffusion.cpp`, `tools/pi-page.sed` (pas de « Cardputer », règle 4 sans nom de touche).

Comme sur Android : `M5Cardputer.Display` est un écran en mémoire de 240 x 135 (`offset_rotation = 3` annule le `setRotation(1)` du jeu) ; juste après chaque image (`mirror::unlockScreen()`), l'image est copiée, le menu des rôles et le clavier à l'écran sont dessinés par-dessus, et un numéro d'image avance ; le fil d'affichage la montre quand le numéro a changé. Le jeu tourne sur son propre fil tant que le programme tourne.

**Mémoire** : `~/.config/explorer3/settings.txt` (langue, record, nom et mot de passe du Wi-Fi, comme la NVS du Cardputer), lisible par son seul propriétaire. Un réseau où le Pi est déjà compte comme le réseau mémorisé (`Preferences.h`) : « Multijoueur » y va alors directement, comme le Cardputer avec son Wi-Fi mémorisé.

**Pont** (`bridge.inc`) : ajouté à la copie de `main.cpp`, il lit les variables internes du jeu (même unité de compilation) sans rien changer : ce que l'écran attend qu'on tape (y compris le nom et le mot de passe du Wi-Fi), les changements d'écran, la liste des Wi-Fi, « Connecter l'écran », « Multijoueur » sélectionné, la langue.

## 6. Textes : clavier, Xbox, PlayStation

Les textes du jeu existent en trois versions, toutes faites à partir de `src/textes.h` :

- **clavier** (`textes`) : `tools/pi-texts.sed` seulement. Les noms des touches du Cardputer restent (ENTRÉE, ESC, TAB, DEL, ESPACE : le clavier d'un Pi les a) ; « Cardputer seul » devient « Solo », « Tout le jeu sur cet appareil » ; « Fn Fn Fn » devient « Ctrl Ctrl Ctrl » ; « CONNEXION AU WI-FI » devient « CONNEXION AU RÉSEAU » (Ethernet aussi) ; « Mémoire insuffisante » devient « Port 80 ou 81 déjà utilisé » (un Pi ne manque pas de mémoire à cet endroit, mais un autre programme peut tenir les ports) ;
- **Xbox** (`textes_xbox`) et **PlayStation** (`textes_ps`) : ensuite `tools/pad-texts.sed` (celui d'Explorer3-Android), dont les marques deviennent :

| Marque | Rôle | Xbox, 8BitDo | PlayStation |
|---|---|---|---|
| `@OK@` | valider (ENTRÉE) | A | × |
| `@BACK@` | retour (ESC) | B | ○ |
| `@KB@` | clavier à l'écran | Y | △ |
| `@SQ@` | ESPACE, effacer | X | □ |
| `@L@` | aide (TAB) | LB | L1 |
| `@R@` | pause (Fn × 3) | Start | Options |

`texts_table.inc` aligne chaque texte des trois versions (`add(textes::X, textes_xbox::X, textes_ps::X)`, une boucle pour les tableaux). Dans la copie de `main.cpp`, `tr()` appelle `padText()` (`bridge.inc`), qui retrouve le texte dans cette table par son adresse et rend la version de `labels::style()`, le dernier appareil utilisé. Le jeu lui-même n'en sait rien.

## 7. Clavier, manettes, clavier à l'écran, choix du rôle

**Clavier** (comme le simulateur PC) : lettres, chiffres et ponctuation dans la disposition du clavier (saisie de texte de SDL), Entrée, Échap (le `` ` `` du Cardputer), Retour arrière et Suppr (DEL), Tab, Espace, flèches (les `;` `.` `,` `/` du Cardputer ; ignorées là où on tape un nom ou un mot de passe), **Ctrl = Fn** (trois fois : pause). Pas de répétition des touches, comme le clavier du Cardputer. Un `` ` `` tapé comme caractère part avec Maj, pour qu'un mot de passe puisse en contenir un (le jeu ne revient en arrière que sur un `` ` `` sans Maj).

**Manettes** (SDL_GameController, boutons par position quelles que soient les inscriptions) :

| Rôle | Touche du Cardputer | Xbox, 8BitDo | PlayStation |
|---|---|---|---|
| Déplacer | `;` `.` `,` `/` | croix, stick gauche | croix, stick gauche |
| Valider | ENTRÉE | A | × |
| Retour | ESC | B | ○ |
| Clavier à l'écran | — | Y | △ |
| Morse, effacer, actualiser (liste des Wi-Fi) | ESPACE, DEL, R | X | □ |
| Aide | TAB | LB, Select (au relâchement) | L1, Share (au relâchement) |
| Pause | Fn × 3 | RB, Start (au relâchement) | R1, Options (au relâchement) |
| Quitter | — | Start + Select | Options + Share |

Le style (Xbox ou PlayStation) vient de `SDL_GameControllerGetType()` ; chaque touche ou bouton règle `labels::style()`.

**Clavier à l'écran** (`onscreen_keyboard.cpp`) : celui d'Android (lettres des énigmes de Morse, lettres du QCM, code de démarrage, clavier codé), plus une page de **texte** pour le nom et le mot de passe du Wi-Fi : trois rangées de caractères (a–z, A–Z ou symboles), puis ABC (majuscules), #+= (symboles), espace, ← (effacer) et OK. À la manette, il s'ouvre tout seul pour le code, le clavier codé et la saisie du Wi-Fi ; il se ferme dès qu'on se sert du clavier.

**Choix du rôle** (`role_menu.cpp`, celui d'Android, touches de l'appareil utilisé) : valider « Multijoueur » ouvre « Équipage » / « Centre de contrôle ». Dans les deux cas le jeu continue vers ses écrans du réseau (section 8). En centre de contrôle, `startServer()` ne démarre rien, et quand le jeu arrive sur « Connecter l'écran » (le réseau est là), `input.cpp` demande l'écran du centre de contrôle ; en sortir ramène le jeu à son menu (deux Échap).

## 8. Réseau

`netctl.cpp` pilote **NetworkManager** par `nmcli` (le réseau de Raspberry Pi OS depuis Bookworm), sur ses propres fils, sans shell (noms et mots de passe passés tels quels en arguments), messages en anglais (`LC_ALL=C`) pour en lire les causes :

- **état** toutes les 2 s (1 s avec le réseau Explorer3) : `nmcli device` ; réseau Explorer3 d'abord, puis Ethernet (« Ethernet »), puis Wi-Fi (nom de la connexion) ; adresse par `getifaddrs()` ;
- **liste des Wi-Fi** (`mirror::startScan`/`pollScan`, les deux passes du Cardputer) : d'abord les réseaux que NetworkManager connaît déjà (`--rescan no`), puis une nouvelle recherche (`--rescan yes`) ; signal 0..100 changé en dBm pour les barres du jeu ; le réseau Explorer3 est d'abord arrêté ;
- **connexion** (`mirror::connect`) : déjà dessus : fait tout de suite ; sinon `nmcli device wifi connect <ssid> password <mdp>`, réessayé avec `hidden yes` pour « Autre réseau » ; « Secrets were required » → mot de passe refusé (le jeu le dit aussitôt), « No network with SSID » → introuvable ; un profil créé par une tentative ratée est supprimé ;
- **réseau Explorer3** : une connexion gardée en mémoire seulement (`save no`), point d'accès, WPA2 (CCMP), `Explorer3` / `Explorer3`, **192.168.4.1/24** comme le Cardputer, `ipv4.method shared` : le dnsmasq de NetworkManager distribue les adresses, et `packaging/dnsmasq-explorer3.conf` lui fait répondre 192.168.4.1 à tous les noms (portail captif, section 9). La création prend quelques secondes et bloque le fil du jeu : le fil d'affichage l'indique pendant ce temps (« Création du réseau Explorer3… »). Appareils connectés : `iw dev <wlan> station dump` ;
- **explorer3.local** : `avahi-publish -a -R explorer3.local <adresse>` tant que le serveur de l'équipage tourne, de nouveau quand l'adresse change.

Droits : `packaging/50-explorer3.rules` (polkit) laisse les membres du groupe `netdev` (le premier utilisateur du Pi en fait partie) se servir de NetworkManager sans mot de passe, aussi depuis la console.

## 9. Multijoueur : l'Équipage

`src/mirror_pi.cpp` est le serveur de `diffusion.cpp` en sockets POSIX (celui d'Explorer3-Android, sur les ports du Cardputer) :

- `http://<adresse>/` (port **80**) : la page (`page.inc`) et `/sym.js`, chaque requête sur un fil éphémère (8 au plus) ;
- `ws://<adresse>:81/` : poignée de main WebSocket, lignes d'écran compressées (25 images/s au plus, seulement les lignes changées, tout l'écran à chaque nouvelle connexion sauf le bandeau), messages `P` (horloge, 250 ms), `T` `S` `R` (son), `K` (pages), `L` (langue), `B` (bandeau), `M` (thème du labyrinthe) ; ping toutes les 5 s, déconnexion après 2 sans réponse ; 5 navigateurs au plus ;
- **portail captif** sur le réseau Explorer3 : une requête dont le `Host` n'est ni 192.168.4.1 ni `explorer3.local` reçoit un 302 vers `http://192.168.4.1/` (`/sym.js` est toujours servi, la recherche du centre de contrôle en a besoin).

Les ports sont ouverts par le fil réseau à la demande du jeu (`startServer`), les deux ou aucun : si un autre programme tient le 80 ou le 81, le jeu affiche « Port 80 ou 81 déjà utilisé » sur sa liste des Wi-Fi. Fermés quand ce Pi devient centre de contrôle.

## 10. Multijoueur : le Centre de contrôle

Tout est dans `src/control/`, en C pur sauf `sdl_draw.c`, `net.c` et `control.c`. Il fait ce que fait la page web du jeu, avec les mêmes messages (section 9) :

- **Protocole** : poignée de main et trames WebSocket (`ws.c`), messages texte (`msg.c`), lignes d'image (`rows.c`), horloge de l'équipage (`clocksync.c`, la plus petite différence des 40 derniers messages, comme la page).
- **Pages** (`panel.c`) : règles, les 4 énigmes, clavier codé, chrono O2 qui passe au rouge sous une minute, cadre rouge à chaque erreur. Le **labyrinthe** montre les planètes ou, avec le message `M`, le thème de la partie : images 16 x 16 et leur palette, noms des cases, texte `p4t` et indice du thème. Morse, astronaute et symboles du clavier codé sont lus dans le jeu compilé avec (`gamedata.h`, `bridge.inc`) : toujours ceux de sa version.
- **Textes** : ceux de l'objet `TX` de la page, mot pour mot, tirés de `page.inc` par `tools/page-texts.awk` (`build/gen/control/page_texts.inc`) ; `prepare.sh` s'arrête si l'un manque. Les textes des écrans avant le flux sont dans `ui.c`.
- **Bandeau** de l'écran de fin (`banner.c`) et **son** (`sound.c` : notes, arrêts, grondement du décollage, comme Web Audio dans la page).
- **Police** : `font_8x16.c`, la police 8x16 (page de code 437) de [libogc2](https://github.com/extremscorner/libogc2), copiée telle quelle avec sa licence zlib ; `text.c` convertit l'UTF-8.


- **Recherche** (`net.c`, celle d'Android, sur son propre fil) : les 254 adresses du /24 du Pi, ports **81** (Cardputer, Pi) et **8081** (équipage Android), vérifiés par `GET /sym.js` sur le 80 ou le 8080. Un équipage trouvé : connexion tout de suite ; plusieurs : liste ; aucun : chercher encore ou taper l'adresse.
- **Saisie d'adresse** (`addr.c`) : flèches, ±1 ; L / R ou - / +, ±10.
- **Flux** : copie de l'écran et pages ; reconnexion toute seule ; Retour revient à la recherche, puis au menu du jeu.
- **Touches** dessinées comme celles de l'appareil utilisé : touches du clavier (Entrée, Échap, Tab, - +), Xbox A B Y LB RB, symboles PlayStation L1 R1.
- Réglages : `~/.config/explorer3/control.conf` (lignes `ip=` : dernière adresse qui a répondu, `lang=` : langue).

## 11. Affichage et son

**Affichage** (`main.cpp`, `control/sdl_draw.c`) : une fenêtre SDL, `SDL_WINDOW_FULLSCREEN_DESKTOP` (`--window` pour les essais), SDL_Renderer (OpenGL ES sur le Pi ; KMSDRM sans bureau), synchro verticale. L'image du jeu, comme la copie de l'écran de l'équipage, est agrandie un nombre entier de fois k **sans lissage** dans une texture cible, puis amenée à sa taille finale (le plus grand 16:9 qui tient, centré) **avec lissage** : seuls les bords des gros pixels s'adoucissent (méthode d'Android). Le centre de contrôle dessine une toile 960 x 540 agrandie pour tenir : rectangles, et police 8x16 agrandie 4 fois puis lissée. Le pointeur de la souris est caché en plein écran.

**Son** : une sortie SDL, 48 kHz stéréo, blocs de 1024 trames. Le haut-parleur du jeu (`audio.cpp`, mélangeur d'Android : 8 canaux, notes sinus, échantillons bruts en boucle) ou, en centre de contrôle, la synthèse de `control/sound.c`, ancrée à « maintenant + 2 blocs ». Cette dernière est calculée en permanence pour que son horloge reste juste. En multijoueur, le jeu de l'équipage envoie ses sons au centre de contrôle, comme sur le Cardputer.

## 12. Paquet et démarrage

`tools/make-deb.sh` (`make deb`) :

| Chemin | Contenu |
|---|---|
| `/usr/bin/explorer3` | le programme ; `postinst` : `setcap cap_net_bind_service=+ep` (ports 80 et 81) |
| `/usr/bin/explorer3-autostart` | `on`, `off`, `status` |
| `/usr/share/applications/explorer3.desktop`, icône | l'entrée du menu *Jeux* |
| `/etc/NetworkManager/dnsmasq-shared.d/explorer3.conf` | portail captif (fichier de configuration) |
| `/usr/share/polkit-1/rules.d/50-explorer3.rules` | NetworkManager sans mot de passe pour `netdev` |

Dépendances : SDL2, NetworkManager, dnsmasq-base, avahi-utils, iw, libcap2-bin.

**Attention** : le fichier dnsmasq vaut pour toutes les connexions *partagées* de NetworkManager, pas seulement le réseau Explorer3 : un autre point d'accès créé sur le même Pi répondrait lui aussi 192.168.4.1 à tous les noms.

`explorer3-autostart on` (avec sudo, pour l'utilisateur qui le lance) : si la cible par défaut est graphique, une copie de l'entrée du menu dans le `~/.config/autostart` de cet utilisateur (le bureau de Raspberry Pi OS la lance) ; sinon `/etc/systemd/system/explorer3.service` : le jeu sur tty1 sous cet utilisateur (`PAMName=login`, donc une vraie session : écran, son, entrées), à la place de `getty@tty1`, relancé quand on quitte le jeu.

## 13. Essais

Aucun pour l'instant. À faire : compiler dans WSL (version amd64 dans une fenêtre, puis le `.deb`), puis sur un Pi 3 et un Pi 400 : Solo au clavier et avec chaque manette ; Équipage avec un navigateur, un appareil Android ; Centre de contrôle face à un Cardputer, un appareil Android ; réseau Explorer3 avec un téléphone (portail captif) ; Ethernet ; bureau et Lite ; `explorer3-autostart` dans les deux sens.

## 14. Suite

- Première compilation et essais (section 13).
