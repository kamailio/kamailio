#!/bin/bash
# test one out of PKG_MEM condition with avpops module

# Copyright (C) 2008 1&1 Internet AG
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

# Needs a default kamailio database setup for mysql

source include/common
source include/require
source include/database

CFG=41.cfg
SRV=5060
UAS=5070
UAC=5080

NR=200
RT=50

if ! (check_sipp && check_kamailio && check_module "db_mysql" && check_mysql); then
	exit 0
fi ;

# add a registrar entry to the db;
$MYSQL "INSERT INTO location (username,contact,socket,user_agent,cseq,q) VALUES (\"foo\",\"sip:foo@127.0.0.1:$UAS\",\"udp:127.0.0.1:$UAS\",\"ser_test\",1,-1);"

$MYSQL "INSERT INTO usr_preferences (uuid, attribute, type, value) VALUES (\"foobar\", \"679\", 0, \"foobar!!!!!\");"

# start

$BIN -L $MOD_DIR -Y $RUN_DIR -P $PIDFILE -w . -f $CFG &> /dev/null
ret=$?

if [ "$ret" -eq 0 ]; then
	sipp -sn uas -bg  -i 127.0.0.1 -m $NR -r $RT -p $UAS &> /dev/null
	sipp -sn uac -s foo 127.0.0.1:$SRV -r $RT -i 127.0.0.1 -m $NR -p $UAC &> /dev/null
	ret=$?
fi;

sleep 1
kill_kamailio
killall -9 sipp > /dev/null 2>&1

$MYSQL "DELETE FROM location WHERE ((contact = \"sip:foo@127.0.0.1:$UAS\") and (user_agent = \"ser_test\"));"
$MYSQL "DELETE FROM usr_preferences;"
exit $ret

