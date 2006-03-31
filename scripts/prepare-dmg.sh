#!/bin/bash

. common.sh

### get binaries

if [ ! -d $BINPKGNAME ]; then
  rm -rf $BINPKGNAME
  svn export -q $REPOBASE/dist $BINPKGNAME
fi

### create r/w image

rm -f $DMGNAME.dmg $DMGNAME.rw.dmg
hdiutil create $DMGNAME.rw.dmg -size $SIZESPEC -fs HFS+ -fsargs "-c c=64,a=16,e=16" -layout SPUD -volname "$VOLNAME"
hdiutil attach $DMGNAME.rw.dmg
cp -R $BINPKGNAME/* "/Volumes/$VOLNAME"
( cd "/Volumes/$VOLNAME/efi/refit" && ./enable.sh )

# EOF
