#!/bin/bash
# configuration with pseudo-variables, transformations and xlog output

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
	grep "from 127.0.0.1:33535, method: REGISTER, transport: UDP:127.0.0.1:5060, user agent: Twinkle/1.0"  $TMPFILE > /dev/null
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
echo $ret

cd ../test

killall -9 openser
rm $TMPFILE

exit $ret