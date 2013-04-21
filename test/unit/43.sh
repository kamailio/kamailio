#!/bin/bash
# check forward functionality in utils module

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

if ! (check_kamailio && check_module "utils" && check_module "db_mysql" && check_mysql); then
	exit 0
fi ;

CFG=43.cfg
TMPFILE=`mktemp -t kamailio-test.XXXXXXXXX`
# setup config
echo "mpath=\"../../modules\"" > $CFG
echo "loadmodule \"../../modules/tm/tm.so\"" >> $CFG
echo "loadmodule \"sl/sl.so\"" >> $CFG
echo "loadmodule \"mi_fifo/mi_fifo.so\"" >> $CFG
echo "loadmodule \"../../modules/utils/utils.so\"" >> $CFG
echo "modparam(\"mi_fifo\", \"fifo_name\", \"/tmp/kamailio_fifo\")" >> $CFG
echo "modparam(\"utils\", \"forward_active\", 1)" >> $CFG
echo "route {sl_send_reply(\"404\", \"forbidden\");}" >> $CFG

$BIN -w . -f $CFG > /dev/null


ret=$?

sleep 1

$CTL fifo forward_list &> /dev/null
$CTL fifo forward_filter 0=REGISTER:INVITE &> /dev/null
$CTL fifo forward_list &> /dev/null
$CTL fifo forward_proxy 0=127.0.0.1:7000 &> /dev/null
$CTL fifo forward_list > $TMPFILE 



tmp=`grep -v "Printing forwarding information:
id switch                         filter proxy
 0 off                    REGISTER:INVITE 127.0.0.1:7000" $TMPFILE`
if [ "$tmp" = "" ] ; then
	ret=0
else
	ret=1
fi ;


if [ "$ret" -eq 0 ] ; then
	sipp -sn uac -s 123456787 127.0.0.1:5060 -i 127.0.0.1 -m 1 -nr &> /dev/null
	ret=$?
	killall sipp &> /dev/null
fi;


if ! [ "$ret" -eq 0 ] ; then
	$CTL fifo forward_switch 0=on
	$CTL fifo forward_list > $TMPFILE
	tmp=`grep -v "Printing forwarding information:
id switch                         filter proxy
 0 on                    REGISTER:INVITE 127.0.0.1:7000" $TMPFILE`
	if [ "$tmp" = "" ] ; then
		ret=0
	else
		ret=1
	fi ;
	nc -l -u -p 7000 1> $TMPFILE 2> /dev/null &
	sipp -sn uac -s 123456787 127.0.0.1:5060 -i 127.0.0.1 -m 1 -nr  &> /dev/null
	killall sipp &> /dev/null
	killall nc &> /dev/null
fi;

NR=`cat $TMPFILE | grep "INVITE sip:" | wc -l`
if [ "$NR" -eq 1 ] ; then
	ret=0
else
	ret=1
fi;

rm $CFG
rm $TMPFILE
$KILL

exit $ret
