#!/bin/bash
# database access and persistent storage for registrar on mysql

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
source include/database

CFG=11.cfg

if ! (check_sipsak && check_kamailio && check_module "db_mysql" && check_mysql); then
	exit 0
fi ;

SIPDOMAIN=127.0.0.1

$BIN -w . -f $CFG -a no > /dev/null
ret=$?

sleep 1

# register two contacts
sipsak -U -C sip:foobar@127.0.0.1 -s sip:49721123456789@$SIPDOMAIN -H $SIPDOMAIN > /dev/null 2>&1
sipsak -U -C sip:foobar1@127.0.0.1 -s sip:49721123456789@$SIPDOMAIN -H $SIPDOMAIN > /dev/null 2>&1
ret=$?

if [ "$ret" -eq 0 ]; then
	$CTL ul show | grep "AOR:: 49721123456789" > /dev/null
	ret=$?
fi;

if [ "$ret" -eq 0 ]; then
	TMP=`$MYSQL "select COUNT(*) from location where username='49721123456789';" | tail -n 1`
	if [ "$TMP" -eq 0 ] ; then
		ret=1
	fi;
fi;

if [ "$ret" -eq 0 ]; then
	# check if the contact is registered
	sipsak -U -C empty -s sip:49721123456789@$SIPDOMAIN -H $SIPDOMAIN -q "Contact: <sip:foobar@127.0.0.1>" > /dev/null 2>&1
	ret=$?
fi;

if [ "$ret" -eq 0 ]; then
	# update the registration
	sipsak -U -C sip:foobar@127.0.0.1 -s sip:49721123456789@$SIPDOMAIN -H $SIPDOMAIN > /dev/null 2>&1
	ret=$?
fi;

if [ "$ret" -eq 0 ]; then
	# check if we get a hint when we try to unregister a non-existent conctact
	sipsak -U -C "sip:foobar2@127.0.0.1" -s sip:49721123456789@$SIPDOMAIN -H $SIPDOMAIN -x 0 -q "Contact: <sip:foobar@127.0.0.1>" > /dev/null 2>&1
	ret=$?
fi;

if [ "$ret" -eq 0 ]; then
	# unregister the contact
	sipsak -U -C "sip:foobar@127.0.0.1" -s sip:49721123456789@$SIPDOMAIN -H $SIPDOMAIN -x 0 > /dev/null 2>&1
	ret=$?
fi;

if [ "$ret" -eq 0 ]; then
	# unregister the user again should not fail
	sipsak -U -C "sip:foobar@127.0.0.1" -s sip:49721123456789@$SIPDOMAIN -H $SIPDOMAIN -x 0 > /dev/null 2>&1
	ret=$?
fi;

if [ "$ret" -eq 0 ]; then
	# check if the other contact is still registered
	sipsak -U -C empty -s sip:49721123456789@$SIPDOMAIN -H $SIPDOMAIN -q "Contact: <sip:foobar1@127.0.0.1>" > /dev/null 2>&1
	ret=$?
fi;

if [ "$ret" -eq 0 ]; then
	# register the other again
	sipsak -U -C sip:foobar@127.0.0.1 -s sip:49721123456789@$SIPDOMAIN -H $SIPDOMAIN > /dev/null 2>&1
	ret=$?
fi;

if [ "$ret" -eq 0 ]; then
	# unregister all contacts
	sipsak -U -C "*" -s sip:49721123456789@$SIPDOMAIN -H $SIPDOMAIN -x 0 > /dev/null 2>&1
	ret=$?
fi;

if [ "$ret" -eq 0 ]; then
	# let the timer cleanup the previous registrations
	sleep 2
	$CTL ul show | grep "AOR:: 49721123456789" > /dev/null
	ret=$?
	if [ "$ret" -eq 0 ]; then
		ret=1
	else
		ret=0
	fi;
fi ;

if [ "$ret" -eq 0 ]; then
	ret=`$MYSQL "select COUNT(*) from location where username='49721123456789';" | tail -n 1`
fi;

if [ "$ret" -eq 0 ]; then
	# test min_expires functionality
	sipsak -U -C sip:foobar@127.0.0.1 -s sip:49721123456789@$SIPDOMAIN -H $SIPDOMAIN -x 2 > /dev/null 2>&1
	ret=$?
fi;

if [ "$ret" -eq 0 ]; then
	sleep 3
	# check if the contact is still registered
	sipsak -U -C empty -s sip:49721123456789@$SIPDOMAIN -H $SIPDOMAIN -q "Contact: <sip:foobar@127.0.0.1>" > /dev/null 2>&1
	ret=$?
fi;

if [ "$ret" -eq 0 ]; then
	# register a few more contacts
	sipsak -U -e 9 -s sip:49721123456789@$SIPDOMAIN -H $SIPDOMAIN > /dev/null 2>&1
fi;

if [ "$ret" -eq 0 ]; then
	# let the timer cleanup the previous registrations
	sleep 3
	# and check
	TMP=`$MYSQL "select COUNT(*) from location where username like '49721123456789%';" | tail -n 1`
	if [ "$TMP" -eq 10 ] ; then
		ret=0
	else
		ret=1
	fi;
fi;

$MYSQL "delete from location where username like '49721123456789%';"

if [ "$ret" -eq 0 ]; then
	# register again
	sipsak -U -C sip:foobar@127.0.0.1 -s sip:49721123456789@$SIPDOMAIN -H $SIPDOMAIN > /dev/null 2>&1
	ret=$?
fi;

$KILL

# restart to test preload_udomain functionality
$BIN -w . -f $CFG -a no> /dev/null
ret=$?

sleep 1

if [ "$ret" -eq 0 ]; then
	# check if the contact is still registered
	sipsak -U -C empty -s sip:49721123456789@$SIPDOMAIN -H $SIPDOMAIN -q "Contact: <sip:foobar@127.0.0.1>" > /dev/null 2>&1
	ret=$?
fi;

# check if the methods value is correct
if [ "$ret" -eq 0 ]; then
	$CTL ul show | grep "Methods:: 4294967295" > /dev/null
	ret=$?
fi;

# cleanup
$MYSQL "delete from location where username like '49721123456789%';"

$KILL

exit $ret
