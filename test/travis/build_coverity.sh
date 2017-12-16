#!/bin/bash
# to be executed inside docker

COV_BUILD_OPTIONS=${COV_BUILD_OPTIONS:-}
RESULTS_DIR=${COV_RESULTS_DIR:-/code/cov-int}
TOOL_BASE=${TOOL_BASE:-/tmp/coverity-scan-analysis}

TOOL_DIR=$(find "${TOOL_BASE}" -type d -name 'cov-analysis*')
export PATH=$TOOL_DIR/bin:$PATH

# Build
echo -e "\033[33;1mRunning Coverity Scan Analysis Tool...\033[0m"
(
	cd /code || exit 1
	COVERITY_UNSUPPORTED=1 cov-build --dir "${RESULTS_DIR}" \
		${COV_BUILD_OPTIONS} ./test/travis/build_travis.sh
	cov-import-scm --dir "${RESULTS_DIR}" --scm git \
		--log "${RESULTS_DIR}"/scm_log.txt 2>&1
)
# if [ -f "${RESULTS_DIR}"/scm_log.txt ]; then
# 	echo -e "\033[33;1mscm_log.txt\033[0m"
# 	cat "${RESULTS_DIR}"/scm_log.txt
# fi
# fix perms
echo "*** fixing perms for ${RESULTS_DIR} ***"
chmod -R a+r "${RESULTS_DIR}"
