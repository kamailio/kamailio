#!/bin/sh
# test cfgutils and pv module

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

. include/common
. include/require.sh

if ! (check_netcat && check_kamailio); then
	exit 0
fi;

if ( have_netcat_quit_timer_patch ); then
	NCOPTS='-q 1'
else
	NCOPTS='-w 2'
fi;

CFG=33.cfg

CORE=$(sysctl kern.corefile 2> /dev/null)
echo $CORE | grep '^kern.corefile: %N\.core' > /dev/null
ret=$?
if [ $ret -eq 0 ] ; then
	CORE='kamailio.core'
else
	CORE='core'
fi

if [ -e $CORE ] ; then
	echo "core file found, not run"
	exit 0
fi;

cp $CFG $CFG.bak

ulimit -c unlimited

$BIN -L $MOD_DIR -Y $RUN_DIR -P $PIDFILE -w . -f $CFG -a no > /dev/null
ret=$?

sleep 1

if [ $ret -eq 0 ] ; then
	$CTL rpc cfgutils.check_config_hash | grep '"result":"Identical hash"' > /dev/null
	ret=$?
fi;

echo " " >> $CFG

if [ $ret -eq 0 ] ; then
	$CTL rpc cfgutils.check_config_hash | grep '"result":"Identical hash"' > /dev/null
	ret=$?
fi

if [ ! $ret -eq 0 ] ; then
	# send a message
	cat register.sip | nc $NCOPTS -u 127.0.0.1 5060 > /dev/null
fi

sleep 1
kill_kamailio 2> /dev/null
ret=$?

if [ $ret -eq 0 ] ; then
	ret=1
else
	ret=0
fi

if [ ! -e $CORE ] ; then
	ret=1
fi
rm -f $CORE
mv $CFG.bak $CFG

exit $ret
