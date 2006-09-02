#!/bin/bash

sudo rm -rf root-main root-fsdriver root-blesser

### set up root for main package

R=root-main

mkdir $R
mkdir $R/efi
cp -R ../dist/efi/refit $R/efi/
cp -R ../dist/efi/tools $R/efi/
cp -R ../dist/*.rtf* $R/efi/
rm -rf $R/efi/tools/drivers

find $R -name .svn -exec rm -rf '{}' ';' -prune

chmod -R g+w $R
sudo chown -R root.admin $R

### set up root for fsdriver package

R=root-fsdriver

mkdir $R
mkdir $R/efi
mkdir $R/efi/tools
cp -R ../dist/efi/tools/drivers $R/efi/tools/

find $R -name .svn -exec rm -rf '{}' ';' -prune

chmod -R g+w $R
sudo chown -R root.admin $R

### set up root for blesser package

R=root-blesser

mkdir $R
mkdir $R/StartupItems
cp -R ../dist/rEFItBlesser $R/StartupItems/

find $R -name .svn -exec rm -rf '{}' ';' -prune

chmod 775 $R
chmod -R g-w $R/StartupItems
sudo chown root.admin $R
sudo chown -R root.wheel $R/StartupItems

# EOF
