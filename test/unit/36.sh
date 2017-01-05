#!/bin/bash
# private memory effiency and fragmentation with ul_dump w/ or w/o PKG_MALLOC

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

cp $CFG $CFG.bak

echo "loadmodule \"db_mysql/db_mysql.so\"" >> $CFG

# 1768 contacts should fit into 1 MB of PKG memory
$BIN -L $MOD_DIR -Y $RUN_DIR -P $PIDFILE -V | grep "PKG_MALLOC" > /dev/null
if [ $? -eq 0 ]; then
	NR=176
else
	NR=277
fi;

COUNTER=0
while [  $COUNTER -lt $NR ]; do
	COUNTER=$(($COUNTER+1))
	$MYSQL "insert into location (username, domain, contact, user_agent) values ('foobar-$COUNTER-$RANDOM', 'local', 'foobar-$COUNTER-$RANDOM@$DOMAIN', '___test___'); \
	insert into location (username, domain, contact, user_agent) values ('foobar-$COUNTER-$RANDOM', 'local', 'foobar-$COUNTER-$RANDOM@$DOMAIN', '___test___'); \
	insert into location (username, domain, contact, user_agent) values ('foobar-$COUNTER-$RANDOM', 'local', 'foobar-$COUNTER-$RANDOM@$DOMAIN', '___test___'); \
	insert into location (username, domain, contact, user_agent) values ('foobar-$COUNTER-$RANDOM', 'local', 'foobar-$COUNTER-$RANDOM@$DOMAIN', '___test___'); \
	insert into location (username, domain, contact, user_agent) values ('foobar-$COUNTER-$RANDOM', 'local', 'foobar-$COUNTER-$RANDOM@$DOMAIN', '___test___'); \
	insert into location (username, domain, contact, user_agent) values ('foobar-$COUNTER-$RANDOM', 'local', 'foobar-$COUNTER-$RANDOM@$DOMAIN', '___test___'); \
	insert into location (username, domain, contact, user_agent) values ('foobar-$COUNTER-$RANDOM', 'local', 'foobar-$COUNTER-$RANDOM@$DOMAIN', '___test___'); \
	insert into location (username, domain, contact, user_agent) values ('foobar-$COUNTER-$RANDOM', 'local', 'foobar-$COUNTER-$RANDOM@$DOMAIN', '___test___'); \
	insert into location (username, domain, contact, user_agent) values ('foobar-$COUNTER-$RANDOM', 'local', 'foobar-$COUNTER-$RANDOM@$DOMAIN', '___test___'); \
	insert into location (username, domain, contact, user_agent) values ('foobar-$COUNTER-$RANDOM', 'local', 'foobar-$COUNTER-$RANDOM@$DOMAIN', '___test___');"
done

$BIN -L $MOD_DIR -Y $RUN_DIR -P $PIDFILE -w . -f $CFG > /dev/null
ret=$?

if [ $ret -eq 0 ]; then
	sleep 1
	for ((i=1;i<=100;i+=1)); do
		tmp=$($CTL fifo ul_dump | grep "User-agent:: ___test___" | wc -l)
		NR_=$(($NR * 10))
		if ! [ $tmp -eq $NR_ ]; then
			ret=1
			break;
		fi;
	done
fi;

sleep 1
kill_kamailio

$MYSQL "delete from location where user_agent = '___test___'"

mv $CFG.bak $CFG

exit $ret
