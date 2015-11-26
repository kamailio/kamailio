#!/bin/sh

# test make cfg with all the gcc versions in gcc_versions.txt
# should be run from the main sip-router directory
# Depends on: test/gcc_version.sh and test/gcc_versions.txt

if [ ! -e test/gcc_version.sh -o ! -r test/gcc_versions.txt -o ! -r Makefile ]
then
	echo "ERROR: wrong path, this test must be run from the main"\
		" sip-router directory"
	exit 1
fi

while read v ; do
	GCC_VERSION=$v make CC=test/gcc_version.sh cfg-defs >/dev/null
	if [ $? -ne 0 -o ! -r config.mak ]; then
		echo "ERROR: make cfg failed for version \"$v\""
		exit 1
	fi
	COMPILER=`egrep -o -- "-DCOMPILER='\"[^\"' ]+ [2-9]\.[0-9]{1,2}(\.[0-9]{1,2})?\"'" config.mak`
	if [ $? -ne 0 -o -z "$COMPILER" ]; then
		echo "ERROR: bad ver: \"$v\" => `egrep -o -- "-DCOMPILER='[^']*'" config.mak`"
		exit 1
	fi
	echo "ok: \"$v\" => $COMPILER"
done < test/gcc_versions.txt
