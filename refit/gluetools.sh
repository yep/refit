#!/bin/sh

cd ../binaries
mkdir -p fat

../refit/fatglue.py fat/Shell.efi bios32/NShell.efi em64t/NShell.efi
for tool in DrawBox.efi dhclient.efi ed.efi edit.efi ftp.efi hexdump.efi \
    hostname.efi ifconfig.efi loadarg.efi ping.efi pppd.efi route.efi \
    tcpipv4.efi which.efi ; do
  ../refit/fatglue.py fat/$tool bios32/$tool em64t/$tool
done

exit 0
