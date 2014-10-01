#!/bin/bash
# database access with fetch_result for usrloc on unixodbc

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

source include/common
source include/require
source include/database

if ! (check_sipp && check_kamailio && check_module "db_unixodbc" && check_unixodbc); then
	exit 0
fi ;


CFG=11.cfg

DOMAIN="local"
# setup 250 contacts
NR=25

cp $CFG $CFG.bak

echo "loadmodule \"$SRC_DIR/modules/db_unixodbc/db_unixodbc.so\"" >> $CFG
echo "modparam(\"usrloc\", \"db_url\", \"unixodbc://kamailio:kamailiorw@localhost/kamailio\")" >> $CFG
echo "modparam(\"usrloc\", \"fetch_rows\", 13)" >> $CFG

# isql unfortunally only allow one statement per line
COUNTER=0
CNT=0
while [  $COUNTER -lt $NR ]; do
	COUNTER=$(($COUNTER+1))
	echo -e "insert into location (id, username, domain, contact, user_agent) values ('$CNT', 'foobar-$RANDOM', '$DOMAIN', 'foobar-$RANDOM@$DOMAIN', '___test___'); \n insert into location (id, username, domain, contact, user_agent) values ('$(($CNT+1))', 'foobar-$RANDOM', '$DOMAIN', 'foobar-$RANDOM@$DOMAIN', '___test___'); \n insert into location (id, username, domain, contact, user_agent) values ('$(($CNT+2))', 'foobar-$RANDOM', '$DOMAIN', 'foobar-$RANDOM@$DOMAIN', '___test___'); \n insert into location (id, username, domain, contact, user_agent) values ('$(($CNT+3))', 'foobar-$RANDOM', '$DOMAIN', 'foobar-$RANDOM@$DOMAIN', '___test___'); \n insert into location (id, username, domain, contact, user_agent) values ('$(($CNT+4))', 'foobar-$RANDOM', '$DOMAIN', 'foobar-$RANDOM@$DOMAIN', '___test___'); \n insert into location (id, username, domain, contact, user_agent) values ('$(($CNT+5))', 'foobar-$RANDOM', '$DOMAIN', 'foobar-$RANDOM@$DOMAIN', '___test___'); \n insert into location (id, username, domain, contact, user_agent) values ('$(($CNT+6))', 'foobar-$RANDOM', '$DOMAIN', 'foobar-$RANDOM@$DOMAIN', '___test___'); \n insert into location (id, username, domain, contact, user_agent) values ('$(($CNT+7))', 'foobar-$RANDOM', '$DOMAIN', 'foobar-$RANDOM@$DOMAIN', '___test___'); \n insert into location (id, username, domain, contact, user_agent) values ('$(($CNT+8))', 'foobar-$RANDOM', '$DOMAIN', 'foobar-$RANDOM@$DOMAIN', '___test___'); \n insert into location (id, username, domain, contact, user_agent) values ('$(($CNT+9))', 'foobar-$RANDOM', '$DOMAIN', 'foobar-$RANDOM@$DOMAIN', '___test___');" | $ISQL
	CNT=$(($CNT+10))
done

$BIN -w . -f $CFG > /dev/null
ret=$?

sleep 2
$KILL

echo "delete from location where user_agent = '___test___'" | $ISQL > /dev/null

mv $CFG.bak $CFG

exit $ret
