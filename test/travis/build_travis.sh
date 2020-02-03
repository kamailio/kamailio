#!/bin/bash
#
# build script for travis CI
# environment based docker container at
# https://hub.docker.com/r/kamailio/pkg-kamailio-docker/
#

set -e

DIST=${DIST:-buster}
CC=${CC:-gcc}

if ! [ -d /code/pkg/kamailio/deb/"${DIST}" ] ; then
	echo "${DIST} not supported"
	exit 1
else
	rm -rf /code/debian
	ln -s /code/pkg/kamailio/deb/"${DIST}" /code/debian
fi

function _clean {
	echo "make clean"
	make -f debian/rules clean
}

function _build {
	echo "make build"
	make -f debian/rules build
}

function _install {
	if [[ "$CC" =~ gcc ]] ; then
		echo "make install"
		make install
	else
		echo "skip make install step"
	fi
}

if [[ "${CC}" =~ clang ]] ; then
	CLANG=$(find /usr/bin -type l -name 'clang-[0-9]*' | sort -r | head -1)
	echo "setting clang to ${CLANG}"
	update-alternatives --install /usr/bin/clang clang "${CLANG}" 1
fi

echo "environment DIST=$DIST CC=$CC"
${CC} --version

_clean
_build
_install
