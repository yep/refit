#!/bin/bash

if [ -z "$1" ]; then
  echo "You must specify the version number!"
  exit 1
fi
VERSION="$1"

sudo rm -rf root-main root-fsdriver root-blesser root-partinsp

### set up welcome files

for SRCFILE in *.in.rtf ; do
  DESTFILE="${SRCFILE%.in.rtf}.rtf"
  sed "s/@VERSION@/$VERSION/" <"$SRCFILE" >"$DESTFILE"
done

### set up root for main package

R=root-main

mkdir $R
mkdir $R/efi
cp -R ../dist/efi/refit $R/efi/
cp -R ../dist/efi/tools $R/efi/
cp -R ../dist/*.rtf* $R/efi/
rm -rf $R/efi/tools/drivers

find $R -name .svn -exec rm -rf '{}' ';' -prune
find $R -name .DS_Store -exec rm -rf '{}' ';'

chmod -R g+w $R
sudo chown -R root:admin $R

### set up root for fsdriver package

R=root-fsdriver

mkdir $R
mkdir $R/efi
mkdir $R/efi/tools
cp -R ../dist/efi/tools/drivers $R/efi/tools/

find $R -name .svn -exec rm -rf '{}' ';' -prune
find $R -name .DS_Store -exec rm -rf '{}' ';'

chmod -R g+w $R
sudo chown -R root:admin $R

### set up root for blesser package

R=root-blesser

mkdir $R
mkdir $R/StartupItems
cp -R ../dist/rEFItBlesser $R/StartupItems/

find $R -name .svn -exec rm -rf '{}' ';' -prune
find $R -name .DS_Store -exec rm -rf '{}' ';'

chmod 775 $R
chmod -R g-w $R/StartupItems
sudo chown root:admin $R
sudo chown -R root:wheel $R/StartupItems

### set up root for partinsp package

R=root-partinsp

mkdir $R
cp -R "../dist/Partition Inspector.app" $R/

find $R -name .svn -exec rm -rf '{}' ';' -prune
find $R -name .DS_Store -exec rm -rf '{}' ';'

chmod -R g+w $R
sudo chown -R root:admin $R

# EOF
