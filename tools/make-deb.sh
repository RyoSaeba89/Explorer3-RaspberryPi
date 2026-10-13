#!/bin/sh
# dist/explorer3_<version>_<arch>.deb from build/explorer3 (make deb).
#   /usr/bin/explorer3                        the game (ports 80 and 81: postinst)
#   /usr/bin/explorer3-autostart              start with the Pi, on or off
#   /usr/share/applications/explorer3.desktop the menu entry
#   /usr/share/icons/hicolor/256x256/apps/explorer3.png
#   /etc/NetworkManager/dnsmasq-shared.d/explorer3.conf   captive portal (Explorer3 network)
#   /usr/share/polkit-1/rules.d/50-explorer3.rules        Wi-Fi without a password
set -e

HERE=$(cd "$(dirname "$0")/.." && pwd)
BUILD=${BUILD:-build}
case "$BUILD" in /*) ;; *) BUILD=$HERE/$BUILD ;; esac
BIN=$BUILD/explorer3
[ -x "$BIN" ] || { echo "make-deb.sh: $BIN not built" >&2; exit 1; }

case "$("${CROSS}gcc" -dumpmachine)" in
    aarch64-*) ARCH=arm64 ;;
    arm-*gnueabihf) ARCH=armhf ;;
    x86_64-*) ARCH=amd64 ;;
    *) echo "make-deb.sh: unknown architecture" >&2; exit 1 ;;
esac
VERSION=${VERSION:-dev}
VERSION=${VERSION#v}
# a version tag (v3.0.0...); else (commit hash, "dev") below any version
case "$VERSION" in [0-9]*.[0-9]*) ;; *) VERSION="0.0~$VERSION" ;; esac
VERSION=$(echo "$VERSION" | tr '_' '~')

ROOT=$BUILD/deb
rm -rf "$ROOT"
P=$HERE/packaging
install -D -m 755 "$BIN" "$ROOT/usr/bin/explorer3"
"${CROSS}strip" "$ROOT/usr/bin/explorer3"
install -D -m 755 "$P/explorer3-autostart" "$ROOT/usr/bin/explorer3-autostart"
install -D -m 644 "$P/explorer3.desktop" "$ROOT/usr/share/applications/explorer3.desktop"
install -D -m 644 "$P/explorer3.png" "$ROOT/usr/share/icons/hicolor/256x256/apps/explorer3.png"
install -D -m 644 "$P/dnsmasq-explorer3.conf" "$ROOT/etc/NetworkManager/dnsmasq-shared.d/explorer3.conf"
install -D -m 644 "$P/50-explorer3.rules" "$ROOT/usr/share/polkit-1/rules.d/50-explorer3.rules"
install -D -m 644 "$HERE/LICENSE" "$ROOT/usr/share/doc/explorer3/copyright"

mkdir -p "$ROOT/DEBIAN"
SIZE=$(du -sk "$ROOT/usr" "$ROOT/etc" | awk '{s += $1} END {print s}')
cat > "$ROOT/DEBIAN/control" <<EOF
Package: explorer3
Version: $VERSION
Architecture: $ARCH
Maintainer: RyoSaeba89 <119814892+RyoSaeba89@users.noreply.github.com>
Installed-Size: $SIZE
Depends: libc6 (>= 2.36), libstdc++6 (>= 12), libgcc-s1, libsdl2-2.0-0 (>= 2.0.18), network-manager, dnsmasq-base, avahi-utils, iw, libcap2-bin
Section: games
Priority: optional
Homepage: https://github.com/RyoSaeba89/Explorer3-RaspberryPi
Description: Explorer 3, a 5-minute escape game (crash on Mars)
 The Escape Game Explorer 3 of the M5Stack Cardputer on the Raspberry Pi 3
 and 400: solo, or multiplayer as the crew (a web browser or another
 device is mission control) or as mission control. Keyboard or
 gamepad, French or English.
EOF
echo "/etc/NetworkManager/dnsmasq-shared.d/explorer3.conf" > "$ROOT/DEBIAN/conffiles"

cat > "$ROOT/DEBIAN/postinst" <<'EOF'
#!/bin/sh
set -e
if [ "$1" = configure ]; then
    # Ports 80 and 81 (page and stream, as on the Cardputer) without running as root
    setcap cap_net_bind_service=+ep /usr/bin/explorer3 || \
        echo "explorer3: setcap failed, the crew's page will not be served" >&2
    if command -v gtk-update-icon-cache >/dev/null 2>&1; then
        gtk-update-icon-cache -q /usr/share/icons/hicolor 2>/dev/null || true
    fi
fi
EOF
cat > "$ROOT/DEBIAN/prerm" <<'EOF'
#!/bin/sh
set -e
if [ "$1" = remove ] && [ -x /usr/bin/explorer3-autostart ]; then
    /usr/bin/explorer3-autostart off >/dev/null 2>&1 || true
fi
EOF
chmod 755 "$ROOT/DEBIAN/postinst" "$ROOT/DEBIAN/prerm"

mkdir -p "$HERE/dist"
OUTFILE=$HERE/dist/explorer3_${VERSION}_${ARCH}.deb
dpkg-deb --root-owner-group --build "$ROOT" "$OUTFILE" >/dev/null
echo "$OUTFILE"
