#!/bin/bash
#
# build script for travis CI
# environment based docker container at
# https://hub.docker.com/r/linuxmaniac/pkg-kamailio-docker/
#

set -e

DIST=${DIST:-stretch}
CC=${CC:-gcc}

if ! [ -d /code/pkg/kamailio/deb/"${DIST}" ] ; then
	echo "${DIST} not supported"
	exit 1
else
	rm -rf /code/debian
	ln -s /code/pkg/kamailio/deb/"${DIST}" /code/debian
fi
function build {
	echo "make clean"
	make -f debian/rules clean
	echo "make build"
	make -f debian/rules build
}

if [[ "${CC}" =~ clang ]] ; then
	CLANG=$(find /usr/bin -type l -name 'clang-[0-9]*' | sort -r | head -1)
	echo "setting clang to ${CLANG}"
	update-alternatives --install /usr/bin/clang clang "${CLANG}" 1
fi

echo "environment DIST=$DIST CC=$CC"
${CC} --version

# build flags
export MEMDBG=0
echo "build with MEMDBG=0"
build

export MEMDBG=1
echo "build with MEMDBG=1"
build

if [[ "$CC" =~ gcc ]] ; then
	echo "make install"
	make install
else
	echo "skip make install step"
fi
