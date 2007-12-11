#! /bin/bash
# test basic accounting functionality

# needs the sipp utility to run
which sipp > /dev/null
ret=$?

if [ ! $? -eq 0 ] ; then
	echo "sipp not found, not run"
	exit 0
fi ;
                
CFG="20.cfg"
TMPFILE=`mktemp -t openser-test.XXXXXXXXXX`

# insert a test user into log db
if [ ! -e $SIPP ] ; then 
	echo "404 - sipp not found"
	exit 0
fi;

# add an registrar entry to the db;
mysql --show-warnings -B -u openser --password=openserrw -D openser -e "INSERT INTO location (username,contact,socket,user_agent,cseq,q) VALUES (\"foo\",\"sip:foo@localhost\",\"udp:127.0.0.1:5060\",\"ser_test\",1,-1);"

sipp -sn uas -bg -i localhost -m 1 -f 10 -p 5060 -nr &> /dev/null

../openser -f $CFG &> $TMPFILE

sipp -sn uac -s foo 127.0.0.1:5059 -i 127.0.0.1 -m 1 -f 10 -p 5061 -nr &> /dev/null

egrep '^ACC:[[:space:]]+transaction[[:space:]]+answered:[[:print:]]*code=200;reason=OK$' $TMPFILE > /dev/null
ret=$?

# cleanup:
killall -9 sipp &> /dev/null
killall -9 openser &> /dev/null
rm $TMPFILE

mysql  --show-warnings -B -u openser --password=openserrw -D openser -e "DELETE FROM location WHERE ((contact = \"sip:foo@localhost\") and (user_agent = \"ser_test\"));"

exit $ret;
