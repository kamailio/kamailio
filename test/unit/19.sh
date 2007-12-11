#!/bin/bash
# check user lockup for proxy functionality with usrloc and registrar

CFG=19.cfg

# needs the sipp utility to run
which sipp > /dev/null
ret=$?

if [ ! $? -eq 0 ] ; then
	echo "sipp not found, not run"
	exit 0
fi ;

# add an registrar entry to the db;
mysql --show-warnings -B -u openser --password=openserrw -D openser -e "INSERT INTO location (username,contact,socket,user_agent,cseq,q) VALUES (\"foo\",\"sip:foo@localhost\",\"udp:127.0.0.1:5060\",\"ser_test\",1,-1);"

../openser -f $CFG &> /dev/null
sipp -sn uas -bg -i localhost -m 10 -f 2 -p 5060 -nr &> /dev/null
sipp -sn uac -s foo 127.0.0.1:5059 -i 127.0.0.1 -m 10 -f 2 -p 5061 -nr &> /dev/null

ret=$?

# cleanup:
killall -9 sipp > /dev/null 2>&1
killall -9 openser > /dev/null 2>&1

mysql  --show-warnings -B -u openser --password=openserrw -D openser -e "DELETE FROM location WHERE ((contact = \"sip:foo@localhost\") and (user_agent = \"ser_test\"));"
exit $ret;
