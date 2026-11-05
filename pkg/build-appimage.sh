#!/bin/sh
# Builds dist/AutoClicker_Advanced-<version>-x86_64.AppImage
# Needs appimagetool and the type2 runtime file. Paths can be given with
# APPIMAGETOOL=... and RUNTIME=... (defaults: in PATH / ./pkg/runtime-x86_64).
set -e
cd "$(dirname "$0")/.."
VERSION=$(sed -n 's/#define XCA_VERSION "\(.*\)"/\1/p' src/version.h)
ID=$(sed -n 's/#define XCA_APP_ID "\(.*\)"/\1/p' src/version.h)
APPDIR=build/AppDir
TOOL=${APPIMAGETOOL:-appimagetool}
RUNTIME=${RUNTIME:-pkg/runtime-x86_64}

make
rm -rf "$APPDIR"
install -Dm755 build/autoclicker-advanced "$APPDIR/usr/bin/autoclicker-advanced"
install -Dm644 assets/$ID.png "$APPDIR/$ID.png"
install -Dm644 assets/$ID.png "$APPDIR/usr/share/icons/hicolor/256x256/apps/$ID.png"
ln -sf "$ID.png" "$APPDIR/.DirIcon"
cat > "$APPDIR/$ID.desktop" <<EOF
[Desktop Entry]
Type=Application
Name=AutoClicker Advanced
GenericName=Autoclicker
Comment=Autoclicker with position steps
Exec=autoclicker-advanced
Icon=$ID
Terminal=false
Categories=Utility;
StartupWMClass=autoclicker-advanced
EOF
install -Dm644 "$APPDIR/$ID.desktop" "$APPDIR/usr/share/applications/$ID.desktop"
cat > "$APPDIR/AppRun" <<'EOF'
#!/bin/sh
HERE="$(dirname "$(readlink -f "$0")")"
export APPDIR="${APPDIR:-$HERE}"
exec "$HERE/usr/bin/autoclicker-advanced" "$@"
EOF
chmod 755 "$APPDIR/AppRun"
DOC="$APPDIR/usr/share/doc/autoclicker-advanced"
for f in LICENSE NOTICE.md README.md THIRD-PARTY-NOTICES.md licenses/*.txt; do
    install -Dm644 "$f" "$DOC/$f"
done

mkdir -p dist
OUT="dist/AutoClicker_Advanced-$VERSION-x86_64.AppImage"
ARCH=x86_64 "$TOOL" --no-appstream --runtime-file "$RUNTIME" "$APPDIR" "$OUT"
echo "built $OUT"
