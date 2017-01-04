#!/bin/bash
# database access and persistent storage for registrar on mysql

# Copyright (C) 2010 marius.zbihlei@1and1.ro
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

CFG=50.cfg

if ! (check_sipsak && check_kamailio && check_module "db_mysql" && check_mysql); then
	exit 0
fi ;

MYSQL_LOC_A="mysql loc_a --show-warnings --batch --user=ser --password=ser -e"
MYSQL_LOC_B="mysql loc_b --show-warnings --batch --user=ser --password=ser  -e"
cp $CFG $CFG.bak

$BIN -L $MOD_DIR -Y $RUN_DIR -P $PIDFILE -w . -f $CFG #> /dev/null
ret=$?

sleep 1

# register two contacts
echo "adding new contacts"
sipsak -U -C sip:foobar@localhost -s sip:49721123456789@localhost -H localhost -x 30 &> /dev/null
sipsak -U -C sip:foobar1@localhost -s sip:49721123456789@localhost -H localhost -x 30 &> /dev/null
sipsak -U -C sip:foobar2@localhost -s sip:49721123456790@localhost -H localhost -x 30 &> /dev/null
ret=$?

if [ ! "$ret" -eq 0 ]; then
    echo "registration failed"
fi

if [ "$ret" -eq 0 ]; then
	TMP=`$MYSQL_LOC_A "select COUNT(*) from location where username='49721123456789';" | tail -n 1`
	if [ "$TMP" -eq 0 ] ; then
		TMP=`$MYSQL_LOC_B "select COUNT(*) from location where username='49721123456789';" | tail -n 1`
		if [ "$TMP" -eq 0 ] ; then 
			echo "User 49721123456789 was NOT saved to either loc_a or loc_b"
			ret=1
		fi
	fi;
fi;

if [ "$ret" -eq 0 ]; then
	TMP=`$MYSQL_LOC_A "select COUNT(*) from location where username='49721123456790';" | tail -n 1`
	if [ "$TMP" -eq 0 ] ; then
		TMP=`$MYSQL_LOC_B "select COUNT(*) from location where username='49721123456790';" | tail -n 1`
		if [ "$TMP" -eq 0 ] ; then 
			echo "User 49721123456790 was NOT saved to either loc_a or loc_b"
			ret=1
		fi
	fi;
fi;

if [ "$ret" -eq 0 ]; then
	echo "check if the contact is registered"
	sipsak -U -C empty -s sip:49721123456789@127.0.0.1 -H localhost -q "Contact: <sip:foobar@localhost>" #&> /dev/null
	ret=$?
	echo "sipsak exited with status $ret"
fi;

if [ "$ret" -eq 0 ]; then
	echo "update the registration"
	sipsak -U -C sip:foobar@localhost -s sip:49721123456789@localhost -H localhost &> /dev/null
	ret=$?
fi;

if [ "$ret" -eq 0 ]; then
	echo "check if we get a hint when we try to unregister a non-existent conctact"
	sipsak -U -C "sip:foobar2@localhost" -s sip:49721123456789@127.0.0.1 -H localhost -x 0 -q "Contact: <sip:foobar@localhost>" &> /dev/null
	ret=$?
fi;

if [ "$ret" -eq 0 ]; then
	echo " unregister the contact"
	sipsak -U -C "sip:foobar@localhost" -s sip:49721123456789@127.0.0.1 -H localhost -x 0 &> /dev/null
	ret=$?
fi;

if [ "$ret" -eq 0 ]; then
	echo "unregister the user again should not fail"
	sipsak -U -C "sip:foobar@localhost" -s sip:49721123456789@127.0.0.1 -H localhost -x 0 #&> /dev/null
	ret=$?
fi;

if [ "$ret" -eq 0 ]; then
	echo "check if the other contact is still registered"
	sipsak -U -C empty -s sip:49721123456789@127.0.0.1 -H localhost -q "Contact: <sip:foobar1@localhost>" &> /dev/null
	ret=$?
fi;

if [ "$ret" -eq 0 ]; then
	echo " register the other again"
	sipsak -U -C sip:foobar@localhost -s sip:49721123456789@localhost -H localhost &> /dev/null
	ret=$?
fi;

if [ "$ret" -eq 0 ]; then
	echo " unregister all contacts"
	sipsak -U -C "*" -s sip:49721123456789@127.0.0.1 -H localhost -x 0 &> /dev/null
	ret=$?
fi;

if [ "$ret" -eq 0 ]; then
	executed=1
	ret=`$MYSQL_LOC_A "select COUNT(*) from location where username='49721123456789';" | tail -n 1`
	if [ "$TMP" -eq 0 ] ; then
		ret=`$MYSQL_LOC_B "select COUNT(*) from location where username='49721123456789';" | tail -n 1`
	fi;
fi;

if [ "x$executed" == "x1" ]; then
    echo "After un-registration user has $ret contacts "
fi

if [ "$ret" -eq 0 ]; then
	# test min_expires functionality
	sipsak -U -C sip:foobar@localhost -s sip:49721123456789@localhost -H localhost -x 2 &> /dev/null
	ret=$?
fi;

if [ "$ret" -eq 0 ]; then
	sleep 3
	# check if the contact is still registered
	sipsak -U -C empty -s sip:49721123456789@127.0.0.1 -H localhost -q "Contact: <sip:foobar@localhost>" &> /dev/null
	ret=$?
fi;

if [ "$ret" -eq 0 ]; then
	# register a few more contacts
	sipsak -U -e 9 -s sip:49721123456789@localhost -H localhost &> /dev/null
fi;


$MYSQL_LOC_A "delete from location where username like '497211234567%';"
$MYSQL_LOC_B "delete from location where username like '497211234567%';"


kill_kamailio

mv $CFG.bak $CFG

exit $ret
