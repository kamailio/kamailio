#!/bin/sh

#$Id$
#
# finds CC version and prints it in the following format:
# compiler_name version major_version
#



if [ $# -lt 1 ]
then 
	echo "Error: you must specify the compiler name" 1>&2
	exit 1
fi

if [ "$1" = "-h" ]
then
	echo "Usage: "
	echo "      $0 compiler_name"
	exit 1
fi


CC=$1

if  which $CC >/dev/null
then
	(test ! -x `which $CC`) && echo "Error: $CC not executable" 1>&2 && exit 1
else
	echo "Error: $CC not found or not executable" 1>&2
	exit 1 
fi


if $CC -v 2>/dev/null 1>/dev/null
then
	FULLVER=`$CC -v 2>&1` 
else
	FULLVER=`$CC -V 2>&1`
fi



if [ -n "$FULLVER" ]
then
	# check if gcc
	if echo "$FULLVER"|grep gcc >/dev/null
	then
		NAME=gcc
		VER=`$CC --version|head -n 1| \
				sed -e 's/^[^0-9]*\([0-9][0-9]*\.[0-9][0-9]*\.[0-9][0-9]*\).*/\1/'\
				    -e 's/^[^.0-9]*\([0-9][0-9]*\.[0-9][0-9]*\).*/\1/'`
	elif echo "$FULLVER"|grep Sun >/dev/null
	then
		NAME=suncc
		VER=`echo "$FULLVER"|head -n 1| \
				sed -e 's/.*\([0-9][0-9]*\.[0-9][0-9]*\).*/\1/'`
	elif echo "$FULLVER"|grep "Intel(R) C++ Compiler" >/dev/null
	then
		NAME=icc
		VER=`echo "$FULLVER"|head -n 1| \
				sed -e 's/.*Version \([0-9]\.[0-9]\.[0-9]*\).*/\1/' ` 
	fi
	
	# find major ver
	if [  -n "$VER"  -a -z "$MAJOR_VER" ]
	then
		MAJOR_VER=`echo "$VER" |cut -d. -f1`
	fi
fi	


#unknown
if [ -z "$NAME" ]
then
	NAME="unknown"
	VER="unknown"
	MAJOR_VER="unknown"
fi


echo "$NAME $VER $MAJOR_VER"
