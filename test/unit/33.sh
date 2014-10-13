#!/bin/bash
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

source include/common
source include/require

if ! (check_netcat && check_kamailio); then
	exit 0
fi;

if [ -e core ] ; then
	echo "core file found, not run"
	exit 0
fi;

CFG=33.cfg

cp $CFG $CFG.bak

ulimit -c unlimited

$BIN -w . -f $CFG -a no > /dev/null
ret=$?

sleep 1

if [ $ret -eq 0 ] ; then
	$CTL mi check_config_hash | grep "The actual config file hash is identical to the stored one." >/dev/null
	ret=$?
fi;

echo " " >> $CFG
if [ $ret -eq 0 ] ; then
	$CTL mi check_config_hash | grep "The actual config file hash is identical to the stored one." >/dev/null
	ret=$?
fi

if [ ! $ret -eq 0 ] ; then
	# send a message
	cat register.sip | nc -q 1 -u 127.0.0.1 5060 > /dev/null
fi

sleep 1
$KILL >/dev/null 2>&1
ret=$?

if [ $ret -eq 0 ] ; then
	ret=1
else
	ret=0
fi

if [ ! -e core ] ; then
	ret=1
fi
rm -f core
mv $CFG.bak $CFG

exit $ret
