#! /bin/bash
# test basic accounting functionality

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

CFG="20.cfg"
TMPFILE=`mktemp -t kamailio-test.XXXXXXXXXX`

# add an registrar entry to the db;
$MYSQL "INSERT INTO location (ruid, username,contact,socket,user_agent,cseq,q) VALUES (\"kamailio-test-uid\",\"foo\",\"sip:foo@127.0.0.1\",\"udp:127.0.0.1:5060\",\"kamailio_test\",1,-1);"

sipp -sn uas -bg -i 127.0.0.1 -m 1 -f 10 -p 5060 &> /dev/null

$BIN -w . -f $CFG > $TMPFILE 2>&1

sipp -sn uac -s foo 127.0.0.1:5059 -i 127.0.0.1 -m 1 -f 10 -p 5061 &> /dev/null

egrep 'ACC:[[:space:]]+transaction[[:space:]]+answered:[[:print:]]*code=200;reason=OK$' $TMPFILE > /dev/null
ret=$?

sleep 1

# cleanup
killall -9 sipp &> /dev/null
$KILL &> /dev/null
rm $TMPFILE

$MYSQL "DELETE FROM location WHERE ((contact = \"sip:foo@127.0.0.1\") and (user_agent = \"kamailio_test\"));"

exit $ret;
