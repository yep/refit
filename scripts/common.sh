# common.sh

if [ -z "$1" ]; then
  echo "You must specify the version number!"
  exit 1
fi

### setup

VERSION="$1"
if [ "$VERSION" = "s" ]; then
  REPOBASE="https://svn.sourceforge.net/svnroot/refit/trunk"
  VERSION="snapshot-$(date '+%Y%m%d')"
else
  REPOBASE="https://svn.sourceforge.net/svnroot/refit/tags/release-$VERSION"
fi

WORKDIR="/tmp/refit-$VERSION"
SRCPKGNAME="refit-src-$VERSION"
BINPKGNAME="refit-bin-$VERSION"
DMGNAME="rEFIt-$VERSION"
VOLNAME="rEFIt"

SIZESPEC="$2"
if [ -z "$SIZESPEC" ]; then
  SIZESPEC="10m"
fi

set -e
mkdir -p $WORKDIR
set -x
cd $WORKDIR

# EOF
