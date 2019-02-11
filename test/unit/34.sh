#!/bin/sh
# load all presence related modules with mysql

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

# Needs a default kamailio database setup for mysql

. include/common
. include/require.sh
. include/database.sh

CFG=34.cfg

if ! (check_kamailio && check_module "db_mysql" && check_module "presence" \
		&& check_module "presence_xml" && check_module "pua" \
		&& check_module "xcap_client" && check_module "rls" \
		&& check_module "presence_mwi" && check_module "pua_bla" \
		&& check_module "pua_usrloc" \
		&& check_module "pua_xmpp" && check_module "xmpp" \
		&& check_module "presence_dialoginfo" && check_module "pua_dialoginfo" \
		&& check_module "pua_rpc" && check_mysql); then
	exit 0
fi ;

cp $CFG $CFG.bak

echo "loadmodule \"db_mysql/db_mysql.so\"" >> $CFG

# start
$BIN -L $MOD_DIR -Y $RUN_DIR -P $PIDFILE -w . -f $CFG -a no > /dev/null
ret=$?

sleep 1
kill_kamailio

mv $CFG.bak $CFG

exit $ret
