#!/bin/bash
# test publish and subscribe for presence

# Copyright (C) 2008 Voice System
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

source include/require

if ! (check_sipp && check_openser && check_module "db_mysql" && check_module "presence" && check_module "presence_xml"); then
	exit 0
fi ;

CFG=presence.cfg

../openser -w . -f $CFG &> /dev/null;
ret=$?
sleep 1

if [ "$ret" -eq 0 ] ; then
    sipp -sf publish_scenario.xml -i 127.0.0.1 -p 5061 -inf publish.csv 127.0.0.1:5059 -recv_timeout 500000 -m 1 &> /dev/null;
    ret=$?
fi;


if [ "$ret" -eq 0 ] ; then
    sipp -sf subscribe_notify_scenario.xml -i 127.0.0.1 -p 5061 -inf subscribe_notify.csv 127.0.0.1:5059 -recv_timeout 500000 -m 1 &> /dev/null;
    ret=$?
fi;


#cleanup:
killall -9 openser &> /dev/null;
killall -9 sipp &> /dev/null;

exit $ret;
