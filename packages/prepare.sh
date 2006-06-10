#!/bin/bash

sudo rm -rf root-blesser

### set up root for Blesser package

mkdir root-blesser
mkdir root-blesser/StartupItems
cp -R ../dist/rEFItBlesser root-blesser/StartupItems/

find root-blesser -name .svn -exec rm -r '{}' ';' -prune

chmod 775 root-blesser
chmod -R g-w root-blesser/StartupItems
sudo chown root.admin root-blesser
sudo chown -R root.wheel root-blesser/StartupItems

# EOF
