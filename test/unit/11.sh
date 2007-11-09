#!/bin/bash
# database access and persistent storage for registrar on mysql

# needs the netcat utility to run

CFG=11.cfg

which nc > /dev/null
ret=$?

if [ ! $ret -eq 0 ] ; then
	echo "netcat not found, not run"
	exit 0
fi ;

../openser -f $CFG > /dev/null
ret=$?

sleep 1

# register a user
cat register.sip | nc -q 1 -u localhost 5060 > /dev/null

cd ../scripts

if [ "$ret" -eq 0 ] ; then
	./openserctl ul show | grep "AOR:: 1000" > /dev/null
	ret=$?
fi ;

TMP=`mysql -B -u openserro --password=openserro openser -e "select COUNT(*) from location where username="1000";" | tail -n 1`
if [ "$TMP" -eq 0 ] ; then
	ret=1
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

ret=`mysql -B -u openserro --password=openserro openser -e "select COUNT(*) from location where username="1000";" | tail -n 1`

cd ../test

killall -9 openser

exit $ret