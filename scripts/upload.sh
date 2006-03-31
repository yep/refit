#!/bin/bash

. common.sh

SAFETY=echo

### upload

for file in $SRCPKGNAME.tar.gz $BINPKGNAME.tar.gz $DMGNAME.dmg $DMGNAME.cdr.gz ; do
  if [ -f $file ]; then
    $SAFETY curl -T $file ftp://upload.sourceforge.net/incoming/
  fi
done

### continue on sourceforge

open 'http://sourceforge.net/project/admin/editpackages.php?group_id=161917'

# EOF
