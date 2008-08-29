#!/bin/bash
# database access and persistent storage for registrar on mysql

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

CFG=11.cfg

if ! (check_netcat && check_kamailio && check_module "db_mysql" && check_mysql); then
	exit 0
fi ;

cp $CFG $CFG.bak

echo "loadmodule \"db_mysql/db_mysql.so\"" >> $CFG

../$BIN -w . -f $CFG > /dev/null
ret=$?

sleep 1

# register a user
cat register.sip | nc -q 1 -u localhost 5060 > /dev/null

cd ../scripts

if [ "$ret" -eq 0 ] ; then
	./$CTL ul show | grep "AOR:: 1000" > /dev/null
	ret=$?
fi ;

TMP=`mysql -B -u openserro --password=openserro openser -e "select COUNT(*) from location where username="1000";" | tail -n 1`
if [ "$TMP" -eq 0 ] ; then
	ret=1
fi ;

# see if the user is registered
cat ../test/invite.sip | nc -q 1 -u localhost 5060 > /dev/null

# unregister the user
cat ../test/unregister.sip | nc -q 1 -u localhost 5060 > /dev/null

if [ "$ret" -eq 0 ] ; then
	./$CTL ul show | grep "AOR:: 1000" > /dev/null
	ret=$?
	if [ "$ret" -eq 0 ] ; then
		ret=1
	else
		ret=0
	fi ;
fi ;

ret=`mysql -B -u openserro --password=openserro openser -e "select COUNT(*) from location where username="1000";" | tail -n 1`

cd ../test

killall -9 $BIN

mv $CFG.bak $CFG

exit $ret