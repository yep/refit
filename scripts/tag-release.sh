#!/bin/bash

if [ -z "$1" ]; then
  echo "You must specify the version number!"
  exit 1
fi

VERSION="$1"
set -x

svn cp https://svn.sourceforge.net/svnroot/refit/trunk \
       https://svn.sourceforge.net/svnroot/refit/tags/release-$VERSION \
       -m "Tagging the $VERSION release."

# EOF
