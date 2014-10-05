#!/bin/bash
# tests the authentification via auth_db

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


CFG=21.cfg

# add an registrar entry to the db;
$MYSQL "INSERT INTO subscriber (username, domain, password) VALUES (\"alice\",\"localhost\",\"alice\");"

$BIN -w . -f $CFG -E -e -dd > /dev/null 2>&1
ret=$?

sleep 1

if [ "$ret" -eq 0 ] ; then
	sipp -s alice 127.0.0.1:5059 -i 127.0.0.1 -m 1 -f 1 -auth_uri alice@localhost -p 5061 -sf reg_auth.xml -ap alice > /dev/null 2>&1 &
	ret=$?
fi

if [ "$ret" -eq 0 ] ; then
	sipp -s alice 127.0.0.1:5059 -i 127.0.0.1 -m 1 -f 1 -auth_uri alice@localhost -p 5061 -sf inv_auth.xml -ap alice > /dev/null 2>&1 &
	ret=$?
fi

sleep 1

#cleanup
$KILL > /dev/null
killall -9 sipp > /dev/null 2>&1
$MYSQL "DELETE FROM subscriber WHERE((username = \"alice\") and (domain = \"localhost\"));"

exit $ret;
