#!/bin/sh

#
# wrapper arroung gcc, that intercepts --version and reports instead
# $GCC_VERSION
#

if [ -n "$GCC_VERSION" ]; then
	for o in $@; do
		if [ "$o" = "--version" ] ; then
			echo $GCC_VERSION
			exit 0
		fi
	done
fi

gcc $@
