#!/bin/bash
#
# build_deps script for travis CI
# installs the build_deps packages needed to build Kamailio
# environment based on Ubuntu 12.04 LTS (precise)
#
DIST=${1:-precise}
BASE_DIR=${2:-$(pwd)}
CONTROL_FILE="${BASE_DIR}/pkg/kamailio/deb/${DIST}/control"
if ! [ -f "${CONTROL_FILE}" ]; then
	echo "Error: No ${CONTROL_FILE} found"
	exit 1
fi

BUILD_DEPS=$(/usr/bin/gdebi --quiet --non-interactive \
	--option=APT::Install-Recommends=false \
	--apt-line ${CONTROL_FILE})
if [ -z "${BUILD_DEPS}" ]; then
	echo "Error: no build deps packages resolved"
	exit 2
fi

apt-get install -y $BUILD_DEPS
