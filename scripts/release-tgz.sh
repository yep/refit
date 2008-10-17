#!/bin/bash

. common.sh

### package source

rm -rf $SRCPKGNAME $SRCPKGNAME.tar $SRCPKGNAME.tar.gz
svn export -q $REPOBASE $SRCPKGNAME
rm -rf $SRCPKGNAME/dist/efi/rescue
tar -cf $SRCPKGNAME.tar $SRCPKGNAME
gzip -9 $SRCPKGNAME.tar

### package binaries

rm -rf $BINPKGNAME $BINPKGNAME.tar $BINPKGNAME.tar.gz
svn export -q $REPOBASE/dist $BINPKGNAME
rm -rf $BINPKGNAME/efi/rescue
tar -cf $BINPKGNAME.tar $BINPKGNAME
gzip -9 $BINPKGNAME.tar

### show size for .dmg packaging

du -sh $BINPKGNAME

# EOF
