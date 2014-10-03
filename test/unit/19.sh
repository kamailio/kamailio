#!/bin/bash
# check user lookup for proxy functionality with usrloc and registrar for mysql

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

CFG=19.cfg
SRV=5060
UAS=5070
UAC=5080


# add an registrar entry to the db;
$MYSQL "INSERT INTO location (ruid,username,contact,socket,user_agent,cseq,q) VALUES (\"kamailio-unit-uid\", \"foo\",\"sip:foo@localhost:$UAS\",\"udp:127.0.0.1:$UAS\",\"kamailio_test\",1,-1);"

$BIN -w . -f $CFG > /dev/null
sipp -sn uas -bg -i localhost -m 10 -f 2 -p $UAS &> /dev/null
sipp -sn uac -s foo 127.0.0.1:$SRV -i localhost -m 10 -f 2 -p $UAC &> /dev/null

ret=$?

# cleanup
killall -9 sipp > /dev/null 2>&1
$KILL > /dev/null 2>&1

$MYSQL "DELETE FROM location WHERE ((contact = \"sip:foo@localhost:$UAS\") and (user_agent = \"kamailio_test\"));"
exit $ret;
