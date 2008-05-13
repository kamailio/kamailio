#!/bin/bash
# loads a userblacklist config from mysql database and test some lists

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

# needs the sipp utility to run
which sipp > /dev/null
ret=$?

if [ ! $? -eq 0 ] ; then
	echo "sipp not found, not run"
	exit 0
fi ;

CFG=25.cfg

MYSQL="mysql openser -u openser --password=openserrw -e"


# add an registrar entry to the db;
$MYSQL "insert into location (username,contact,socket,user_agent,cseq,q) values (\"49721123456789\",\"sip:123456789@localhost\",\"udp:127.0.0.1:5060\",\"ser_test\",1,-1);"

$MYSQL "insert into location (username,contact,socket,user_agent,cseq,q) values (\"49721123456788\",\"sip:123456788@localhost\",\"udp:127.0.0.1:5060\",\"ser_test\",1,-1);"

$MYSQL "insert into location (username,contact,socket,user_agent,cseq,q) values (\"49721123456787\",\"sip:123456787@localhost\",\"udp:127.0.0.1:5060\",\"ser_test\",1,-1);"

$MYSQL "insert into location (username,contact,socket,user_agent,cseq,q) values (\"49721123456786\",\"sip:123456786@localhost\",\"udp:127.0.0.1:5060\",\"ser_test\",1,-1);"

$MYSQL "insert into location (username,contact,socket,user_agent,cseq,q) values (\"49721123456785\",\"sip:223456789@localhost\",\"udp:127.0.0.1:5060\",\"ser_test\",1,-1);"


# setup userblacklist, first some dummy data
$MYSQL "insert into userblacklist (username, domain, prefix, whitelist, description) values ('494675454','','49900','0','_test_');"
$MYSQL "insert into userblacklist (username, domain, prefix, whitelist, description) values ('494675453','test.domain','49901','0','_test_');"
$MYSQL "insert into userblacklist (username, domain, prefix, whitelist, description) values ('494675231','test','499034132','0','_test_');"
$MYSQL "insert into userblacklist (username, domain, prefix, whitelist, description) values ('494675231','','499034133','1','_test_');"
# some actual data
$MYSQL "insert into userblacklist (username, domain, prefix, whitelist, description) values ('49721123456789','','12345','0','_test_');"
$MYSQL "insert into userblacklist (username, domain, prefix, whitelist, description) values ('49721123456788','','123456788','1','_test_');"
$MYSQL "insert into userblacklist (username, domain, prefix, whitelist, description) values ('49721123456788','','1234','0','_test_');"
# and the global ones
$MYSQL "insert into globalblacklist (prefix, whitelist, description) values ('123456787','0','_test_');"
$MYSQL "insert into globalblacklist (prefix, whitelist, description) values ('123456','0','_test_');"
$MYSQL "insert into globalblacklist (prefix, whitelist, description) values ('1','1','_test_');"
$MYSQL "insert into globalblacklist (prefix, whitelist, description) values ('','0','_test_');"


../openser -w . -f $CFG &> /dev/null
sleep 1

sipp -sn uas -bg -i localhost -p 5060 &> /dev/null

sipp -sn uac -s 49721123456789 127.0.0.1:5059 -i 127.0.0.1 -m 1 -f 2 -p 5061 &> /dev/null
ret=$?

if [ "$ret" -eq 1 ] ; then
	sipp -sn uac -s 49721123456788 127.0.0.1:5059 -i 127.0.0.1 -m 1 -f 2 -p 5061 &> /dev/null
	ret=$?
fi;

if [ "$ret" -eq 1 ] ; then
	sipp -sn uac -s 49721123456787 127.0.0.1:5059 -i 127.0.0.1 -m 1 -f 2 -p 5061 &> /dev/null
	ret=$?
fi;

if [ "$ret" -eq 1 ] ; then
	sipp -sn uac -s 49721123456786 127.0.0.1:5059 -i 127.0.0.1 -m 1 -f 2 -p 5061 &> /dev/null
	ret=$?
fi;

$MYSQL "insert into globalblacklist (prefix, whitelist, description) values ('123456786','1','_test_');"
../scripts/openserctl fifo reload_blacklist

if [ "$ret" -eq 1 ] ; then
	sipp -sn uac -s 49721123456786 127.0.0.1:5059 -i 127.0.0.1 -m 1 -f 2 -p 5061 &> /dev/null
	ret=$?
fi;

$MYSQL "insert into userblacklist (username, domain, prefix, whitelist, description) values ('49721123456786','','12345','0','_test_');"

if [ "$ret" -eq 0 ] ; then
	sipp -sn uac -s 49721123456786 127.0.0.1:5059 -i 127.0.0.1 -m 1 -f 2 -p 5061 &> /dev/null
	ret=$?
fi;

if [ "$ret" -eq 1 ] ; then
	sipp -sn uac -s 49721123456785 127.0.0.1:5059 -i 127.0.0.1 -m 1 -f 2 -p 5061 &> /dev/null
	ret=$?
fi;

$MYSQL "insert into globalblacklist (prefix, whitelist, description) values ('2','1','_test_');"
../scripts/openserctl fifo reload_blacklist

if [ "$ret" -eq 1 ] ; then
	sipp -sn uac -s 49721123456785 127.0.0.1:5059 -i 127.0.0.1 -m 1 -f 2 -p 5061 &> /dev/null
	ret=$?
fi;


# cleanup:
killall -9 sipp > /dev/null 2>&1
killall -9 openser > /dev/null 2>&1

$MYSQL "delete from location where (user_agent = \"ser_test\");"
$MYSQL "delete from userblacklist where description='_test_';"
$MYSQL "delete from globalblacklist where description='_test_';"

exit $ret;
