#!/bin/bash
# do some routing with carrierroute route sets from mysql database

# Copyright (C) 2007 1&1 Internet AG
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

CFG=26.cfg

if [ ! -e ../modules/carrierroute/carrierroute.so ] ; then
	echo "carrierroute not found, not run"
	exit 0
fi ;

cp $CFG $CFG.bak

# setup config
echo "loadmodule \"db_mysql/db_mysql.so\"" >> $CFG
echo "modparam(\"carrierroute\", \"config_source\", \"db\")" >> $CFG

# setup database
MYSQL="mysql openser -u openser --password=openserrw -e"

$MYSQL "insert into route_tree (id, carrier) values ('1', 'default');"
$MYSQL "insert into route_tree (id, carrier) values ('2', 'carrier1');"
$MYSQL "insert into route_tree (id, carrier) values ('3', 'carrier2');"

$MYSQL "insert into carrierroute (id, carrier, domain, scan_prefix, flags, prob, strip, rewrite_host)
values ('1','1','0','49','0','0.5','0','127.0.0.1:7000');"
$MYSQL "insert into carrierroute (id, carrier, domain, scan_prefix, flags, prob, strip, rewrite_host)
values ('2','1','0','49','0','0.5','0','127.0.0.1:8000');"
$MYSQL "insert into carrierroute (id, carrier, domain, scan_prefix, flags, prob, strip, rewrite_host)
values ('3','2','0','49','0','1','0','127.0.0.1:9000');"
$MYSQL "insert into carrierroute (id, carrier, domain, scan_prefix, flags, mask,prob, strip, rewrite_host)
values ('4','3','0','49','0', '3', '1','0','127.0.0.1:10001');"
$MYSQL "insert into carrierroute (id, carrier, domain, scan_prefix, flags, mask,prob, strip, rewrite_host)
values ('5','3','0','49','1', '3', '1','0','127.0.0.1:10000');"
$MYSQL "insert into carrierroute (id, carrier, domain, scan_prefix, flags, mask, prob, strip, rewrite_host)
values ('6','3','0','49','2', '3', '1','0','127.0.0.1:10002');"
$MYSQL "insert into carrierroute (id, carrier, domain, scan_prefix, flags, prob, strip, rewrite_host)
values ('7','3','fallback','49','0','1','0','127.0.0.1:10000');"
$MYSQL "insert into carrierroute (id, carrier, domain, scan_prefix, flags, prob, strip, rewrite_host)
values ('8','3','2','49','0','1','0','127.0.0.1:10000');"

$MYSQL "insert into carrierfailureroute(id, carrier, domain, scan_prefix, host_name, reply_code,
flags, mask, next_domain) values ('1', '3', '0', '49', '127.0.0.1:10000', '5..', '', '', 'fallback');"
$MYSQL "insert into carrierfailureroute(id, carrier, domain, scan_prefix, host_name, reply_code,
flags, mask, next_domain) values ('2', '3', 'fallback', '49', '127.0.0.1:10000', '483', '', '', '2');"
$MYSQL "insert into carrierfailureroute(id, carrier, domain, scan_prefix, host_name, reply_code,
flags, mask, next_domain) values ('3', '3', 'fallback', '49', '127.0.0.1:9000', '4..', '', '', '2');"

$MYSQL "alter table subscriber add cr_preferred_carrier int(10) default NULL;"

$MYSQL "insert into subscriber (username, cr_preferred_carrier) values ('49721123456786', 2);"
$MYSQL "insert into subscriber (username, cr_preferred_carrier) values ('49721123456785', 3);"


../openser -w . -f $CFG > /dev/null

ret=$?

sleep 1

if [ "$ret" -eq 0 ] ; then
	sipp -sn uas -bg -i localhost -m 12 -p 7000 &> /dev/null
	sipp -sn uas -bg -i localhost -m 12 -p 8000 &> /dev/null
	sipp -sn uac -s 49721123456787 127.0.0.1:5060 -i 127.0.0.1 -m 20 -p 5061 &> /dev/null
	ret=$?
	killall sipp &> /dev/null
fi;

if [ "$ret" -eq 0 ] ; then
	sipp -sn uas -bg -i localhost -m 10 -p 9000 &> /dev/null
	sipp -sn uac -s 49721123456786 127.0.0.1:5060 -i 127.0.0.1 -m 10 -p 5061 &> /dev/null
	ret=$?
fi;

if [ "$ret" -eq 0 ] ; then
	sipp -sn uas -bg -i localhost -m 10 -p 10000 &> /dev/null
	sipp -sn uac -s 49721123456785 127.0.0.1:5060 -i 127.0.0.1 -m 10 -p 5061 &> /dev/null
	ret=$?
fi;

if [ "$ret" -eq 0 ] ; then
	killall sipp &> /dev/null
	sipp -sf failure_route.xml -bg -i localhost -m 10 -p 10000 &> /dev/null
	sipp -sn uac -s 49721123456785 127.0.0.1:5060 -i 127.0.0.1 -m 10 -p 5061 &> /dev/null
	ret=$?
fi;

$MYSQL "insert into carrierfailureroute(id, carrier, domain, scan_prefix, host_name, reply_code,
flags, mask, next_domain) values ('4', '3', 'fallback', '49', '127.0.0.1:10000', '4..', '', '', '4');"
$MYSQL "insert into carrierfailureroute(id, carrier, domain, scan_prefix, host_name, reply_code,
flags, mask, next_domain) values ('5', '3', 'fallback', '49', '127.0.0.1:10000', '486', '', '', '2');"

if [ ! "$ret" -eq 0 ] ; then
	../scripts/openserctl fifo cr_reload_routes
	killall sipp &> /dev/null
	sipp -sf failure_route.xml -bg -i localhost -m 10 -p 10000 &> /dev/null
	sipp -sn uac -s 49721123456785 127.0.0.1:5060 -i 127.0.0.1 -m 10 -p 5061 &> /dev/null
	ret=$?
fi;


killall -9 openser
killall -9 sipp

# cleanup database
$MYSQL "delete from route_tree where id = 1;"
$MYSQL "delete from route_tree where id = 2;"
$MYSQL "delete from route_tree where id = 3;"
$MYSQL "delete from carrierroute where carrier=1;"
$MYSQL "delete from carrierroute where carrier=2;"
$MYSQL "delete from carrierroute where carrier=3;"
$MYSQL "delete from carrierfailureroute where carrier=1;"
$MYSQL "delete from carrierfailureroute where carrier=2;"
$MYSQL "delete from carrierfailureroute where carrier=3;"

$MYSQL "delete from subscriber where username='49721123456786';"
$MYSQL "delete from subscriber where username='49721123456785';"
$MYSQL "alter table subscriber drop cr_preferred_carrier;"

cd ../test

mv $CFG.bak $CFG

exit $ret