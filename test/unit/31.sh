#!/bin/bash
# database access with fetch_result for usrloc on mysql

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

if ! (check_kamailio && check_module "db_mysql" && check_mysql); then
	exit 0
fi ;

CFG=11.cfg

DOMAIN="local"
# setup 500 contacts
NR=50

cp $CFG $CFG.bak

echo "loadmodule \"$SRC_DIR/modules/db_mysql/db_mysql.so\"" >> $CFG
echo "modparam(\"usrloc\", \"fetch_rows\", 13)" >> $CFG

COUNTER=0
while [  $COUNTER -lt $NR ]; do
	COUNTER=$(($COUNTER+1))
	$MYSQL "insert into location (username, domain, contact, user_agent) values ('foobar-$RANDOM', '$DOMAIN', 'foobar-$RANDOM@$DOMAIN', '___test___'); insert into location (username, domain, contact, user_agent) values ('foobar-$RANDOM', '$DOMAIN', 'foobar-$RANDOM@$DOMAIN', '___test___'); insert into location (username, domain, contact, user_agent) values ('foobar-$RANDOM', '$DOMAIN', 'foobar-$RANDOM@$DOMAIN', '___test___'); insert into location (username, domain, contact, user_agent) values ('foobar-$RANDOM', '$DOMAIN', 'foobar-$RANDOM@$DOMAIN', '___test___'); insert into location (username, domain, contact, user_agent) values ('foobar-$RANDOM', '$DOMAIN', 'foobar-$RANDOM@$DOMAIN', '___test___'); insert into location (username, domain, contact, user_agent) values ('foobar-$RANDOM', '$DOMAIN', 'foobar-$RANDOM@$DOMAIN', '___test___'); insert into location (username, domain, contact, user_agent) values ('foobar-$RANDOM', '$DOMAIN', 'foobar-$RANDOM@$DOMAIN', '___test___'); insert into location (username, domain, contact, user_agent) values ('foobar-$RANDOM', '$DOMAIN', 'foobar-$RANDOM@$DOMAIN', '___test___'); insert into location (username, domain, contact, user_agent) values ('foobar-$RANDOM', '$DOMAIN', 'foobar-$RANDOM@$DOMAIN', '___test___'); insert into location (username, domain, contact, user_agent) values ('foobar-$RANDOM', '$DOMAIN', 'foobar-$RANDOM@$DOMAIN', '___test___');"
done

$BIN -w . -f $CFG > /dev/null
ret=$?

sleep 1
$KILL

$MYSQL "delete from location where user_agent = '___test___'"

mv $CFG.bak $CFG

exit $ret
