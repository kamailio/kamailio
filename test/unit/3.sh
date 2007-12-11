#!/bin/bash
# creates a mysql database with openserdbctl and deletes it again

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

# Needs a mysql database, the root user password must be given
# in the file 'dbrootpw' in the test directory

if [ ! -f dbrootpw ] ; then
	echo "no root password, not run"
	exit 0
fi ;

source dbrootpw

tmp_name=""$RANDOM"_openserdb_tmp"

cd ../scripts

# setup config file
cp openserctlrc openserctlrc.bak
sed -i "s/# SIP_DOMAIN=openser.org/SIP_DOMAIN=sip.localhost/g" openserctlrc
sed -i "s/# DBENGINE=MYSQL/DBENGINE=MYSQL/g" openserctlrc
sed -i "s/# INSTALL_EXTRA_TABLES=ask/INSTALL_EXTRA_TABLES=yes/g" openserctlrc
sed -i "s/# INSTALL_PRESENCE_TABLES=ask/INSTALL_PRESENCE_TABLES=yes/g" openserctlrc
sed -i "s/# INSTALL_SERWEB_TABLES=ask/INSTALL_SERWEB_TABLES=yes/g" openserctlrc

# set the mysql root password
cp openserdbctl.mysql openserdbctl.mysql.bak
sed -i "s/#PW=""/PW="$PW"/g" openserdbctl.mysql

./openserdbctl create $tmp_name > /dev/null
ret=$?

if [ "$ret" -eq 0 ] ; then
	./openserdbctl drop $tmp_name > /dev/null
	ret=$?
fi ;

# cleanup
mv openserctlrc.bak openserctlrc
cp openserdbctl.mysql.bak openserdbctl.mysql

cd ../test
exit $ret