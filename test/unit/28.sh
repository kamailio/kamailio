#!/bin/bash
# tests simple cpl-c script operations with mysql

# Copyright (C) 2008 1&1 Internet AG
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
source include/database

if ! (check_sipp && check_openser && check_module "db_mysql" && check_module "cpl-c" && check_mysql); then
	exit 0
fi ;

CFG=28.cfg
CPL=cpl_ignore.xml
TMPFILE=`mktemp -t openser-test.XXXXXXXXXX`


../openser -w . -f $CFG &> /dev/null;
ret=$?
sleep 1

../scripts/openserctl fifo LOAD_CPL sip:alice@127.0.0.1 $CPL

if [ "$ret" -eq 0 ] ; then
	sipp -m 1 -f 1 127.0.0.1:5060 -sf cpl_test.xml &> /dev/null;
	ret=$?
fi;

if [ "$ret" -eq 0 ] ; then
  ../scripts/openserctl fifo GET_CPL sip:alice@127.0.0.1 > $TMPFILE 
  diff $TMPFILE $CPL 
  ret=$?
fi; 

if [ "$ret" -eq 0 ] ; then
  ../scripts/openserctl fifo REMOVE_CPL sip:alice@127.0.0.1
  ../scripts/openserctl fifo GET_CPL sip:alice@127.0.0.1 > $TMPFILE
fi;

diff $TMPFILE $CPL &> /dev/null;
ret=$?

if [ ! "$ret" -eq 0 ] ; then
  ret=0
fi;

#cleanup:
killall -9 openser &> /dev/null;
killall -9 sipp &> /dev/null;
rm $TMPFILE

exit $ret;
