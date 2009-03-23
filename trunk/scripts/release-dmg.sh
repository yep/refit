#!/bin/bash

. common.sh

### package .dmg

if [ -d "/Volumes/$VOLNAME" ]; then
  hdiutil detach "/Volumes/$VOLNAME"
fi

rm -f $DMGNAME.dmg
hdiutil convert $DMGNAME.rw.dmg -format UDZO -o $DMGNAME.dmg -imagekey zlib-level=9

### package .cdr.gz

rm -f $DMGNAME.cdr $DMGNAME.cdr.gz
hdiutil convert $DMGNAME.rw.dmg -format UDTO -o $DMGNAME.cdr
gzip -9 $DMGNAME.cdr

# EOF
