#!/bin/bash
# load all modules without external dependencies with mysql

# Copyright (C) 2007 1&1 Internet AG
#
# This file is part of openser, a free SIP server.
#
# openser is free software; you can redistribute it and/or modify
# it under the terms of the GNU General Public License as published by
# the Free Software Foundation; either version 2 of the License, or
# (at your option) any later version
#
# openser is distributed in the hope that it will be useful,
# but WITHOUT ANY WARRANTY; without even the implied warranty of
# MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
# GNU General Public License for more details.
#
# You should have received a copy of the GNU General Public License
# along with this program; if not, write to the Free Software
# Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA  02110-1301, USA.

# Needs a default openser database setup for mysql

if [ ! -e ../modules/mysql/mysql.so ] ; then
	echo "mysql driver not found, not run"
	exit 0
fi ;

CFG=2.cfg
cp $CFG $CFG.bak

touch dispatcher.list

echo "loadmodule \"mysql/mysql.so\"" >> $CFG
echo "modparam(\"dispatcher\", \"list_file\", \"`pwd`/../test/dispatcher.list\")" >> $CFG

# start
../openser -f $CFG > /dev/null
ret=$?

sleep 1
killall -9 openser

mv $CFG.bak $CFG
rm -f dispatcher.list

exit $ret