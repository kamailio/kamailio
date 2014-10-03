#!/bin/bash
# load all modules without external dependencies with db_berkeley

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

if ! (check_kamailio && check_module "db_berkeley" ); then
	exit 0
fi ;

CFG=17.cfg

tmp_name=""$RANDOM"_kamailiodb_tmp"

echo "loadmodule \"../../modules/db_berkeley/db_berkeley.so\"" > $CFG
cat 2.cfg >> $CFG
echo "modparam(\"$DB_ALL_MOD\", \"db_url\", \"berkeley://`pwd`/$CTL_DIR/$tmp_name\")" >> $CFG
echo -e "\nrequest_route {\n ;\n}" >> $CFG

# setup config file
cp $CTLRC $CTLRC.bak

sed -i'' -e "s/# DBENGINE=MYSQL/DBENGINE=DB_BERKELEY/g" $CTLRC
sed -i'' -e "s/# INSTALL_EXTRA_TABLES=ask/INSTALL_EXTRA_TABLES=yes/g" $CTLRC
sed -i'' -e "s/# INSTALL_PRESENCE_TABLES=ask/INSTALL_PRESENCE_TABLES=yes/g" $CTLRC
sed -i'' -e "s/# INSTALL_DBUID_TABLES=ask/INSTALL_DBUID_TABLES=yes/g" $CTLRC

cp $DBCTL $DBCTL.bak
sed -i'' -e "s/TEST=\"false\"/TEST=\"true\"/g" $DBCTL

$DBCTL create $tmp_name > /dev/null
ret=$?

if [ "$ret" -eq 0 ] ; then
	$BIN -w . -f $CFG > /dev/null
	ret=$?
fi ;

sleep 1
$KILL

# cleanup
$DBCTL drop $tmp_name > /dev/null
mv $CTLRC.bak $CTLRC
mv $DBCTL.bak $DBCTL

rm $CFG

exit $ret
