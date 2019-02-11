#!/bin/bash
# load all modules without external dependencies with unixodbc

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

# Needs a default kamailio database setup for unixodbc

if ! (check_kamailio && check_module "db_unixodbc" && check_unixodbc); then
	exit 0
fi ;

CFG=2.cfg
cp $CFG $CFG.bak

echo "loadmodule \"db_unixodbc/db_unixodbc.so\"" >> $CFG
echo "modparam(\"$DB_ALL_MOD\", \"db_url\", \"unixodbc://kamailioro:kamailioro@localhost/kamailio\")" >> $CFG

# start
$BIN -L $MOD_DIR -Y $RUN_DIR -P $PIDFILE -w . -f $CFG > /dev/null
ret=$?

sleep 1
kill_kamailio

mv $CFG.bak $CFG
rm -f dispatcher.list

exit $ret
