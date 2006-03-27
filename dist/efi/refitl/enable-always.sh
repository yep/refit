#!/bin/sh

DIR="$(pwd)"
FILE="$DIR/refitl.efi"

if [ ! -f "$FILE" ]; then
  echo "You must run enable.sh from the directory where you put refitl.efi!"
  exit 1
fi
if [ "$(uname -p)" != "i386" ]; then
  echo "You must run enable.sh on an Intel-based Macintosh!"
  exit 1
fi

set -x
sudo bless --folder "$DIR" --file "$FILE" --labelfile refit.vollabel --setBoot
