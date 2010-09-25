#!/bin/sh
# Build a tarball from the latest upstream version, with a nice
# version number.
#
# Requires git 1.6.6 or later, GNU date, and gzip.

set -e

: ${VERSION=201007}

[ -f xburst-tools_${VERSION}.tar.bz2 ] || wget http://projects.qi-hardware.com/media/upload/xburst-tools/files/xburst-tools_${VERSION}.tar.bz2
mkdir -p get-orig-source
echo `pwd`
cd get-orig-source && tar -jxf ../xburst-tools_${VERSION}.tar.bz2
rm -rf get-orig-source/xburst-tools/debian get-orig-source/xburst-tools/.git
cd get-orig-source && tar -czf ../../xburst-tools_${VERSION}-1.orig.tar.gz xburst-tools/
rm -rf xburst-tools_${VERSION}.tar.bz2 get-orig-source