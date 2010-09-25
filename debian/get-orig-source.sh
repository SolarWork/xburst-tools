#!/bin/sh
# Build a tarball from the latest upstream version, with a nice
# version number.
#
# Requires tar.

set -e

: ${VERSION=201007}

mkdir -p debian-orig-source
trap 'rm -fr debian-orig-source xburst-tools_${VERSION}.tar.bz2 || exit 1' EXIT INT TERM

[ -f xburst-tools_${VERSION}.tar.bz2 ] || \
	wget http://projects.qi-hardware.com/media/upload/xburst-tools/files/xburst-tools_${VERSION}.tar.bz2

tar -jxf xburst-tools_${VERSION}.tar.bz2 -C debian-orig-source
rm -rf debian-orig-source/debian
cd debian-orig-source && tar -czf ../../xburst-tools_${VERSION}.orig.tar.gz . && cd ..

rm -fr debian-orig-source xburst-tools_${VERSION}.tar.bz2
