#!/bin/bash
# configuration with pseudo-variables, transformations and xlog output

# Copyright (C) 2007 1&1 Internet AG
#
# This file is part of openser, a free SIP server.
#
# openser is free software; you can redistribute it and/or modify
# it under the terms of the GNU General Public License as published by
# the Free Software Foundation; either version 2 of the License, or
# (at your option) any later version
#
# openser is distributed in the hope that it will be useful,
# but WITHOUT ANY WARRANTY; without even the implied warranty of
# MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
# GNU General Public License for more details.
#
# You should have received a copy of the GNU General Public License
# along with this program; if not, write to the Free Software
# Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA  02110-1301, USA.

# needs the netcat utility to run

CFG=12.cfg
TMPFILE=`mktemp -t openser-test.XXXXXXXXXX`

which nc > /dev/null
ret=$?

if [ ! $? -eq 0 ] ; then
	echo "netcat not found, not run"
	exit 0
fi ;

../openser -f $CFG &> $TMPFILE
ret=$?

sleep 1

# register a user
cat register.sip | nc -q 1 -u localhost 5060 > /dev/null

cd ../scripts

if [ "$ret" -eq 0 ] ; then
	./openserctl ul show | grep "AOR:: 1000" > /dev/null
	ret=$?
fi ;

# unregister the user
cat ../test/unregister.sip | nc -q 1 -u localhost 5060 > /dev/null

if [ "$ret" -eq 0 ] ; then
	./openserctl ul show | grep "AOR:: 1000" > /dev/null
	ret=$?
	if [ "$ret" -eq 0 ] ; then
		ret=1
	else
		ret=0
	fi ;
fi ;

if [ "$ret" -eq 0 ] ; then
	grep "method: REGISTER, transport: UDP:127.0.0.1:5060, user agent: Twinkle/1.0"  $TMPFILE > /dev/null
	ret=$?
	if [ "$ret" -eq 0 ] ; then
		grep "Getting identity from FROM URI" $TMPFILE > /dev/null
		ret=$?
		if [ "$ret" -eq 0 ] ; then
			grep "My identity: sip:1000@127.0.0.1" $TMPFILE > /dev/null
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
					fi ;
				fi ;
			fi ;
		fi ;
	fi ;
fi ;

cd ../test

killall -9 openser
rm $TMPFILE

exit $ret