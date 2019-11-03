#!/bin/sh

OPK_NAME=mgba.opk

echo ${OPK_NAME}

# create default.gcw0.desktop
cat > default.gcw0.desktop <<EOF
[Desktop Entry]
Name=mgba
Comment=Nintendo Gameboy Advance emulator
Exec=mgba %f
Terminal=false
Type=Application
StartupNotify=true
Icon=icon
Categories=emulators;
X-OD-NeedsDownscaling=true
EOF

# create opk
FLIST="build/sdl_legacy/mgba"
FLIST="${FLIST} default.gcw0.desktop"
FLIST="${FLIST} dingoo/icon.png"

rm -f ${OPK_NAME}
mksquashfs ${FLIST} ${OPK_NAME} -all-root -no-xattrs -noappend -no-exports

cat default.gcw0.desktop
rm -f default.gcw0.desktop
