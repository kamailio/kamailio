#!/bin/sh

#$Id$
#
# returns the CFLAGS for the given compiler (maximum optimizations)
#


ARCH=`uname -m |sed -e s/i.86/i386/ -e s/sun4u/sparc64/ `

# gcc 3.x optimize for:
x86CPU=athlon
WARN_ARCH="WARNING: Not tested on architecture $ARCH, using default flags"

if [ $# -lt 1 ]
then 
	echo "ERROR: you must specify the compiler name" 1>&2
	exit 1
fi

if [ "$1" = "-h" ]
then
	echo "Usage: "
	echo "      $0 compiler_name"
	exit 1
fi


if  CCVER=`./ccver.sh $1` 
then
	NAME=`echo "$CCVER"|cut -d" " -f 1`
	VER=`echo "$CCVER"|cut -d" " -f 2`
	MAJOR_VER=`echo "$CCVER"|cut -d" " -f 3`
else
	echo "ERROR executing ./ccver.sh" 2>&1
	exit 1
fi

echo "name=$NAME, ver=$VER, mver=$MAJOR_VER"
case $NAME
in
gcc) 
		#common stuff
		CFLAGS="-O9 -funroll-loops -Winline -Wall"
		case $MAJOR_VER
		in
			3)
				case $ARCH
				in
					i386)
						CFLAGS="$CFLAGS -minline-all-stringops -malign-double"
						CFLAGS="$CFLAGS -falign-loops -march=$x86CPU"
						;;
					sparc64)
						CFLAGS="$CFLAGS -mcpu=ultrasparc -mtune=ultrasparc"
						CFLAGS="$CFLAGS -m32"
						#other interesting options:
						# -mcpu=v9 or ultrasparc? # -mtune implied by -mcpu
						#-mno-epilogue #try to inline function exit code
						#-mflat # omit save/restore
						#-faster-structs #faster non Sparc ABI structure copy
						;;
					armv4l)
						CFLAGS="$CFLAGS -mcpu=strongarm1100"
						;;
						*)
						echo "$WARN_ARCH" 1>&2
						;;
				esac
				;;
			2|*)
				case $ARCH
				in
					i386)
						CFLAGS="$CFLAGS -m486 -malign-loops=4"
						;;
					sparc64)
						CFLAGS="$CFLAGS -mv8 -Wa,-xarch=v8plus"
						;;
					armv4l)
						;;
						*)
						echo "$WARN_ARCH" 1>&2
						;;
				esac
				;;
		esac
		;;

icc)
	CFLAGS="-O3 -ipo -ipo_obj -unroll"
	case $ARCH
	in
		i386)
			CFLAGS="$CFLAGS -tpp6 -xK"
			#-openmp  #optimize for PIII 
			# -prefetch doesn't seem to work
			#( ty to inline acroos files, unroll loops,prefetch,
			# optimize for PIII, use PIII instructions & vect.,
			# mutlithread loops)
		;;
		*)
			echo "$WARN_ARCH" 1>&2
		;;
	esac
	;;

suncc)
	CFLAGS="-xO5 -fast -native -xCC -xc99"
	case $ARCH
	in
		sparc64)
			CFLAGS="$CFLAGS -xarch=v8plusa"
			;;
		*)
			echo "$WARN_ARCH" 1>&2
			;;
	esac
	;;

*)
	echo "WARNING: unknown compiler $NAME, trying _very_ generic flags" 1>&2
	CFLAGS="-O2"
esac


echo "CFLAGS=$CFLAGS"
