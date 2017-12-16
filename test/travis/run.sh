#!/bin/bash
set -e  # abort on first failure
#set -x  # debug aid

TRAVIS_BUILD_DIR=${TRAVIS_BUILD_DIR:-$(pwd)}
DIST=${DIST:-sid}
CC=${CC:-clang}
TRAVIS_BRANCH=${TRAVIS_BRANCH:-master}

DOCKER_TAG_PREFIX=dev
if [[ ${TRAVIS_BRANCH} =~ ^[0-9]+\.[0-9]+$ ]]; then
	DOCKER_TAG_PREFIX=${TRAVIS_BRANCH}
fi

DOCKER_TAG=${DOCKER_TAG:-${DOCKER_TAG_PREFIX}-$DIST}
DOCKER_IMAGE=${DOCKER_IMAGE:-kamailio/pkg-kamailio-docker}
# needed on test/travis/coverity_scan.sh
COVERITY_SCAN_PROJECT_NAME="kamailio/kamailio"
#COVERITY_SCAN_NOTIFICATION_EMAIL="kadmin.pkgci@lists.kamailio.org"
COVERITY_SCAN_NOTIFICATION_EMAIL="linuxmaniac@torreviejawireless.org"
COVERITY_SCAN_BRANCH_PATTERN=coverity_scan

function run_docker {
	echo "DIST=$DIST" > ./env-file
	echo "CC=$CC" >> ./env-file
	docker run --env-file ./env-file \
		-v "${TRAVIS_BUILD_DIR}":/code:rw \
		"${DOCKER_IMAGE}:${DOCKER_TAG}" \
		/bin/bash -c "cd /code; ./test/travis/build_travis.sh"
	rm ./env-file
}

if  [ "${TRAVIS_BRANCH}" != "${COVERITY_SCAN_BRANCH_PATTERN}" ]; then
	docker pull "${DOCKER_IMAGE}:${DOCKER_TAG}"
	run_docker
else
	if [ "$DIST" = sid ] && [ "$CC" = clang ] ; then
		echo -e "\033[33;1mcoverity scan for DIST=$DIST CC=$CC\033[0m"
		source "${TRAVIS_BUILD_DIR}"/test/travis/coverity_scan.sh
	else
		echo -e "\033[33;1mavoid coverity scan build for DIST=$DIST CC=$CC\033[0m"
	fi
fi
