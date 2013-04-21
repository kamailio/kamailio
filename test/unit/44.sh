#!/bin/bash
# database access and persistent storage for registrar on unixodbc

# Copyright (C) 2009 1&1 Internet AG
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
source include/database

if ! (check_sipsak && check_kamailio && check_module "db_unixodbc" && check_unixodbc); then
	exit 0
fi ;

CFG=11.cfg

cp $CFG $CFG.tmp
echo "loadmodule \"$SR_DIR/modules/db_unixodbc/db_unixodbc.so\"" >> $CFG
echo "modparam(\"usrloc\", \"db_url\", \"unixodbc://kamailio:kamailiorw@localhost/kamailio\")" >> $CFG

$BIN -w . -f $CFG > /dev/null
ret=$?

sleep 1
# register a user
sipsak -U -C sip:foobar@localhost -s sip:49721123456789@localhost -H localhost &> /dev/null
ret=$?

if [ "$ret" -eq 0 ]; then
	$CTL ul show | grep "AOR:: 49721123456789" > /dev/null
	ret=$?
fi;

if [ "$ret" -eq 0 ]; then
	TMP=`echo "select COUNT(*) from location where username='49721123456789';" | $ISQL`
	if [ "$TMP" -eq 0 ] ; then
		ret=1
	fi;
fi;

if [ "$ret" -eq 0 ]; then
	# unregister the user
	sipsak -U -C "*" -s sip:49721123456789@127.0.0.1 -H localhost -x 0 &> /dev/null
fi;

if [ "$ret" -eq 0 ]; then
	$CTL ul show | grep "AOR:: 49721123456789" > /dev/null
	ret=$?
	if [ "$ret" -eq 0 ]; then
		ret=1
	else
		ret=0
	fi;
fi;

if [ "$ret" -eq 0 ]; then
	ret=`echo "select COUNT(*) from location where username='49721123456789';" | $ISQL`
fi;

echo "delete from location where username like '49721123456789%';" | $ISQL

if [ "$ret" -eq 0 ]; then
	# register again
	sipsak -U -C sip:foobar@localhost -s sip:49721123456789@localhost -H localhost &> /dev/null
	ret=$?
fi;

$KILL

# restart to test preload_udomain functionality
$BIN -w . -f $CFG > /dev/null
ret=$?

sleep 1

if [ "$ret" -eq 0 ]; then
	# check if the contact is still registered
	sipsak -U -C empty -s sip:49721123456789@127.0.0.1 -H localhost -q "Contact: <sip:foobar@localhost>" &> /dev/null
	ret=$?
fi;

# check if the methods value is correct
if [ "$ret" -eq 0 ]; then
	$CTL ul show | grep "Methods:: 4294967295" &> /dev/null
	ret=$?
fi;

$KILL

echo "delete from location where username like '49721123456789%';" | $ISQL

mv $CFG.tmp $CFG

exit $ret
