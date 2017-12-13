#!/bin/bash
# creates a postgres database with kamailiodbctl and deletes it again

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

. include/common

if [ ! -f ~/.pgpass ] ; then
       echo "no .pgpass file, not run"
       exit 0
fi ;

tmp_name=""$RANDOM"_kamailiodb_tmp"

cd ../../utils/kamctl/

# setup config file
cp $CTLRC $CTLRC.bak
sed -i "s/# DBENGINE=MYSQL/DBENGINE=PGSQL/g" $CTLRC
sed -i "s/# INSTALL_EXTRA_TABLES=ask/INSTALL_EXTRA_TABLES=yes/g" $CTLRC
sed -i "s/# INSTALL_PRESENCE_TABLES=ask/INSTALL_PRESENCE_TABLES=yes/g" $CTLRC

cp $DBCTL $DBCTL.bak
sed -i "s/TEST=\"false\"/TEST=\"true\"/g" $DBCTL

./$DBCTL create $tmp_name &> /dev/null
ret=$?

if [ "$ret" -eq 0 ] ; then
	./$DBCTL drop $tmp_name &> /dev/null
	ret=$?
fi ;

# cleanup
mv $CTLRC.bak $CTLRC
mv $DBCTL.bak $DBCTL

cd -
exit $ret
