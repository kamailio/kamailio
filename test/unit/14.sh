#!/bin/bash
# loads a carrierroute config for loadbalancing from config file

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

CFG=14.cfg

if ! (check_kamailio && check_module "carrierroute" ); then
	exit 0
fi ;

cp $CFG $CFG.bak

$BIN -w . -f $CFG -a no > /dev/null
ret=$?

sleep 1

TMPFILE=`mktemp -t kamailio-test.XXXXXXXXXX`

if [ "$ret" -eq 0 ] ; then
	$CTL fifo cr_dump_routes > $TMPFILE
	ret=$?
fi ;

if [ "$ret" -eq 0 ] ; then
	tmp=`grep -v "Printing routing information:
Printing tree for carrier 'default' (1)
Printing tree for domain 'register' (1)
      NULL: 0.000 %, 'test1': OFF, '0', '', '', ''
      NULL: 50.000 %, 'test2.localdomain': ON, '0', '', '', ''
      NULL: 50.000 %, 'test3.localdomain': ON, '0', '', '', ''
Printing tree for domain 'proxy' (2)
         2: 87.610 %, 'test7.localdomain': ON, '0', '', '', ''
         2: 12.516 %, 'test8.localdomain': ON, '0', '', '', ''
        42: 70.070 %, 'test4': ON, '0', '', '', ''
        42: 20.020 %, 'test5.localdomain': ON, '0', '', '', ''
        42: 10.010 %, 'test6.localdomain': ON, '0', '', '', ''
        49: 0.000 %, 'test5.localdomain': OFF, '0', '', '', ''
        49: 44.444 %, 'test4': ON, '0', '', '', ''
        49: 55.556 %, 'test6.localdomain': ON, '0', '', '', ''
Printing tree for domain 'other' (3)
      NULL: 0.000 %, 'test1': OFF, '0', '', '', ''
      NULL: 50.000 %, 'test2.localdomain': OFF, '0', '', '', ''
            Rule is backed up by: test3.localdomain
      NULL: 50.000 %, 'test3.localdomain': ON, '0', '', '', ''
            Rule is backup for: test2.localdomain" $TMPFILE`
	if [ "$tmp" = "" ] ; then
		ret=0
	else
		ret=1
	fi ;
fi ;

$KILL

mv $CFG.bak $CFG
rm -f $TMPFILE

exit $ret
