#!/bin/sh

for binary in refit/refit dbounce/dbounce dumpfv/dumpfv dumpprot/dumpprot \
    fsw/fsw_ext2 fsw/fsw_iso9660 fsw/fsw_reiserfs gptsync/gptsync \
    TextMode/textmode ; do
  ./efilipo/efilipo -create \
    -output $(basename $binary).efi \
    -arch i386 ${binary}_bios32.efi \
    -arch x86_64 ${binary}_em64t.efi
done

exit 0
