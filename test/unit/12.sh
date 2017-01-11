#!/bin/sh
# configuration with pseudo-variables, transformations and xlog output

# Copyright (C) 2007 1&1 Internet AG
#
# This file is part of Kamailio, a free SIP server.
#
# Kamailio is free software; you can redistribute it and/or modify
# it under the terms of the GNU General Public License as published by
# the Free Software Foundation; either version 2 of the License, or
# (at your option) any later version
#
# Kamailio is distributed in the hope that it will be useful,
# but WITHOUT ANY WARRANTY; without even the implied warranty of
# MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
# GNU General Public License for more details.
#
# You should have received a copy of the GNU General Public License
# along with this program; if not, write to the Free Software
# Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA  02110-1301, USA.

. include/common
. include/require.sh

CFG=12.cfg
TMPFILE=`mktemp -t kamailio-test.XXXXXXXXXX`

if ! (check_netcat && check_kamailio); then
	exit 0
fi ;

$BIN -L $MOD_DIR -Y $RUN_DIR -P $PIDFILE -w . -f $CFG -a no > $TMPFILE 2>&1
ret=$?

if ( have_netcat_quit_timer_patch ); then
	NCOPTS='-q 1'
else
	NCOPTS='-w 1'
fi

sleep 1

if [ "$ret" -eq 0 ] ; then
	# register a user
	cat register.sip | nc $NCOPTS -u localhost 5060 > /dev/null
	$CTL ul show | grep '"AoR":"1000"' > /dev/null
	ret=$?
	# unregister the user
	cat unregister.sip | nc $NCOPTS -u localhost 5060 > /dev/null
fi ;

if [ "$ret" -eq 0 ] ; then
	$CTL ul show | grep '"AoR":"1000"' > /dev/null
	ret=$?
	if [ "$ret" -eq 0 ] ; then
		ret=1
	else
		ret=0
	fi ;
fi ;

if [ "$ret" -eq 0 ] ; then
	grep "method: register, transport: UDP:127.0.0.1:5060, user agent: Twinkle/1.0"  $TMPFILE > /dev/null
	ret=$?
	if [ "$ret" -eq 0 ] ; then
		grep "Getting identity from FROM URI" $TMPFILE > /dev/null
		ret=$?
		if [ "$ret" -eq 0 ] ; then
			grep "My identity: sip:1000@127.0.0.1" $TMPFILE > /dev/null
			ret=$?
			if [ "$ret" -eq 0 ] ; then
				grep "var(x): sip:1000@127.0.0.1, MD5 var(x): b6c3d120cfd9e85addf64ee8943f4eec" $TMPFILE > /dev/null
				ret=$?
				if [ "$ret" -eq 0 ] ; then
					grep "Contact header field present" $TMPFILE > /dev/null
					ret=$?
					if [ "$ret" -eq 0 ] ; then
						grep "this is an registration" $TMPFILE > /dev/null
						ret=$?
						if [ "$ret" -eq 0 ] ; then
							grep "this is an unregistration" $TMPFILE > /dev/null
							ret=$?
						fi;
					fi ;
				fi ;
			fi ;
		fi ;
	fi ;
fi ;

kill_kamailio
rm $TMPFILE

exit $ret
