#!/bin/sh

cd ../binaries
mkdir -p fat

../refit/efilipo/efilipo -create \
  -output fat/shell.efi \
  -arch i386 bios32/NShell.efi \
  -arch x86_64 em64t/NShell.efi

for tool in drawbox.efi dhclient.efi ed.efi edit.efi ftp.efi hexdump.efi \
    hostname.efi ifconfig.efi loadarg.efi ping.efi pppd.efi route.efi \
    tcpipv4.efi which.efi ; do
  ../refit/efilipo/efilipo -create \
    -output fat/$tool \
    -arch i386 bios32/$tool \
    -arch x86_64 em64t/$tool
done

exit 0
