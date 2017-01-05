#!/bin/bash
# load all modules without external dependencies with dbtext

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
. include/require.sh

CFG=15.cfg

if ! (check_kamailio); then
	exit 0
fi ;

printf "loadmodule \"db_text/db_text.so\"" > $CFG
cat 2.cfg >> $CFG
printf "modparam(\"$DB_ALL_MOD\", \"db_url\", \"text://`pwd`/../../utils/kamctl/dbtext/kamailio\")" >> $CFG
printf "\nrequest_route {\n ;\n}" >> $CFG

$BIN -L $MOD_DIR -Y $RUN_DIR -P $PIDFILE -w . -f $CFG -a no > /dev/null
ret=$?

sleep 1
kill_kamailio

rm $CFG

exit $ret
