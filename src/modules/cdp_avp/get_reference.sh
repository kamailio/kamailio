#!/bin/bash
#
# CDiameter AVP Help reference generator
#

gcc -E -DCDP_AVP_REFERENCE get_reference.h | tr '|' '\n' |  egrep "$1" |grep -v -e "^$" -e "#" -e "typedef" | awk '{gsub(/^[ \t]+|[ \t]+$/,"")};1' | sort

