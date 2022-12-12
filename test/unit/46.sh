#!/bin/bash
#Check configuration framework implementation in k modules

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
. include/database.sh

if ! (check_sipp && check_kamailio && check_module "db_mysql" && check_mysql ); then
	exit 0
fi ;

CFG=11.cfg
SRV=5060
UAS=5070
UAC=5080

test_module_int()
{
	if [ x$# != x3 ] ; then 
		echo "wrong number of params : usage test_module module cfg_param value"
		return 1
	fi

	MODNAME="$1"
	MODPARAM="$2"
	MODVAL="$3"

	if [ x$ret = x0 ]  ; then 
		$SERCMD cfg.set_now_int $MODNAME $MODPARAM $MODVAL &>/dev/null
		ret=$?
	fi

	if [ x$ret = x0 ] ; then
		val=$($SERCMD cfg.get $MODNAME $MODPARAM)
		ret=$?

		if [ x"$val" = x$MODVAL ] ; then 
#		    echo "check ok"
		    ret=0;
		else
#		    echo "check failed"
		    ret=1
		fi
	fi

	return $ret
}

# add a registrar entry to the db
cp $CFG ${CFG}.bak

echo "loadmodule \"db_mysql/db_mysql.so\"" >>$CFG
echo "loadmodule \"ctl/ctl.so\"" >> $CFG
echo "loadmodule \"cfg_rpc/cfg_rpc.so\"" >> $CFG
$BIN -L $MOD_DIR -Y $RUN_DIR -P $PIDFILE -w . -f $CFG &> /dev/null
ret=$?
sleep 1

SERCMD="../../utils/sercmd/sercmd"

if [ ! -x $SERCMD ] ; then
    ret=1
    echo "SERCMD executable not found"
fi

test_module_int registrar default_expires 100

ret=$?

# cleanup
killall -9 sipp > /dev/null 2>&1
kill_kamailio

mv ${CFG}.bak $CFG

exit $ret;
