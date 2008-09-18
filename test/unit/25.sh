#!/bin/bash
# loads a userblacklist config from mysql database and test some lists

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

source include/common
source include/require
source include/database

if ! (check_sipp && check_kamailio && check_module "db_mysql" && check_mysql); then
	exit 0
fi ;

CFG=25.cfg

# add an registrar entry to the db;
$MYSQL "insert into location (username,contact,socket,user_agent,cseq,q) values (\"49721123456789\",\"sip:123456789@localhost\",\"udp:127.0.0.1:5060\",\"ser_test\",1,-1);"

$MYSQL "insert into location (username,contact,socket,user_agent,cseq,q) values (\"49721123456788\",\"sip:123456788@localhost\",\"udp:127.0.0.1:5060\",\"ser_test\",1,-1);"

$MYSQL "insert into location (username,contact,socket,user_agent,cseq,q) values (\"49721123456787\",\"sip:123456787@localhost\",\"udp:127.0.0.1:5060\",\"ser_test\",1,-1);"

$MYSQL "insert into location (username,contact,socket,user_agent,cseq,q) values (\"49721123456786\",\"sip:123456786@localhost\",\"udp:127.0.0.1:5060\",\"ser_test\",1,-1);"

$MYSQL "insert into location (username,contact,socket,user_agent,cseq,q) values (\"49721123456785\",\"sip:223456789@localhost\",\"udp:127.0.0.1:5060\",\"ser_test\",1,-1);"


# setup userblacklist, first some dummy data
$MYSQL "insert into userblacklist (username, domain, prefix, whitelist) values ('494675454','','49900','0');"
$MYSQL "insert into userblacklist (username, domain, prefix, whitelist) values ('494675453','test.domain','49901','0');"
$MYSQL "insert into userblacklist (username, domain, prefix, whitelist) values ('494675231','test','499034132','0');"
$MYSQL "insert into userblacklist (username, domain, prefix, whitelist) values ('494675231','','499034133','1');"
# some actual data
$MYSQL "insert into userblacklist (username, domain, prefix, whitelist) values ('49721123456789','','12345','0');"
$MYSQL "insert into userblacklist (username, domain, prefix, whitelist) values ('49721123456788','','123456788','1');"
$MYSQL "insert into userblacklist (username, domain, prefix, whitelist) values ('49721123456788','','1234','0');"
# and the global ones
$MYSQL "insert into globalblacklist (prefix, whitelist, description) values ('123456787','0','_test_');"
$MYSQL "insert into globalblacklist (prefix, whitelist, description) values ('123456','0','_test_');"
$MYSQL "insert into globalblacklist (prefix, whitelist, description) values ('1','1','_test_');"
$MYSQL "insert into globalblacklist (prefix, whitelist, description) values ('','0','_test_');"


../$BIN -w . -f $CFG &> /dev/null
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
../scripts/$CTL fifo reload_blacklist

if [ "$ret" -eq 1 ] ; then
	sipp -sn uac -s 49721123456786 127.0.0.1:5059 -i 127.0.0.1 -m 1 -f 2 -p 5061 &> /dev/null
	ret=$?
fi;

$MYSQL "insert into userblacklist (username, domain, prefix, whitelist) values ('49721123456786','','12345','0');"

if [ "$ret" -eq 0 ] ; then
	sipp -sn uac -s 49721123456786 127.0.0.1:5059 -i 127.0.0.1 -m 1 -f 2 -p 5061 &> /dev/null
	ret=$?
fi;

if [ "$ret" -eq 1 ] ; then
	sipp -sn uac -s 49721123456785 127.0.0.1:5059 -i 127.0.0.1 -m 1 -f 2 -p 5061 &> /dev/null
	ret=$?
fi;

$MYSQL "insert into globalblacklist (prefix, whitelist, description) values ('2','1','_test_');"
../scripts/$CTL fifo reload_blacklist

if [ "$ret" -eq 1 ] ; then
	sipp -sn uac -s 49721123456785 127.0.0.1:5059 -i 127.0.0.1 -m 1 -f 2 -p 5061 &> /dev/null
	ret=$?
fi;


# cleanup:
killall -9 sipp > /dev/null 2>&1
$KILL > /dev/null 2>&1

$MYSQL "delete from location where (user_agent = \"ser_test\");"
$MYSQL "delete from userblacklist where username='49721123456786';"
$MYSQL "delete from userblacklist where username='49721123456788';"
$MYSQL "delete from userblacklist where username='49721123456789';"
$MYSQL "delete from userblacklist where username='494675231';"
$MYSQL "delete from userblacklist where username='494675453';"
$MYSQL "delete from userblacklist where username='494675454';"
$MYSQL "delete from globalblacklist where description='_test_';"

exit $ret;
