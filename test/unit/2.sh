#!/bin/bash
# load all modules without external dependencies with mysql

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

# Needs a default kamailio database setup for mysql

source include/common
source include/require
source include/database

CFG=2.cfg

if ! (check_kamailio && check_module "db_mysql" && check_mysql); then
	exit 0
fi ;

cp $CFG $CFG.bak

touch dispatcher.list

echo "loadmodule \"$SRC_DIR/modules/db_mysql/db_mysql.so\"" >> $CFG
echo "modparam(\"dispatcher\", \"list_file\", \"$SRC_DIR/$TEST_DIR/dispatcher.list\")" >> $CFG
echo -e "\nrequest_route {\n ;\n}" >> $CFG

# start
$BIN -w . -f $CFG > /dev/null
ret=$?

sleep 1
$KILL

mv $CFG.bak $CFG
rm -f dispatcher.list

exit $ret
