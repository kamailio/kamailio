#!/bin/bash
# do some routing with carrierroute route sets from mysql database

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

. include/common
. include/require.sh
. include/database.sh

if ! (check_kamailio && check_module "carrierroute" && check_module "db_mysql" && check_mysql); then
	exit 0
fi ;

CFG=26.cfg

cp $CFG $CFG.bak

# setup config
echo "loadmodule \"db_mysql/db_mysql.so\"" >> $CFG
echo "modparam(\"carrierroute\", \"config_source\", \"db\")" >> $CFG

# setup database
$MYSQL "insert into carrier_name (id, carrier) values ('1', 'default');"
$MYSQL "insert into carrier_name (id, carrier) values ('2', 'carrier1');"
$MYSQL "insert into carrier_name (id, carrier) values ('3', 'carrier2');"

$MYSQL "insert into domain_name (id, domain) values ('10', 'domain0');"
$MYSQL "insert into domain_name (id, domain) values ('1', 'fallback');"
$MYSQL "insert into domain_name (id, domain) values ('2', 'domain2');"

$MYSQL "insert into carrierroute (id, carrier, domain, scan_prefix, flags, mask, prob, strip, rewrite_host)
values ('1','1','10','49','0','0','0.5','0','127.0.0.1:7000');"
$MYSQL "insert into carrierroute (id, carrier, domain, scan_prefix, flags, mask, prob, strip, rewrite_host, description)
values ('2','1','10','49','0','0','0.5','0','127.0.0.1:8000', 'foobar');"
$MYSQL "insert into carrierroute (id, carrier, domain, scan_prefix, flags, mask, prob, strip, rewrite_host)
values ('3','2','10','49','0','0','1','0','127.0.0.1:9000');"
$MYSQL "insert into carrierroute (id, carrier, domain, scan_prefix, flags, mask, prob, strip, rewrite_host)
values ('4','3','10','49','0','3','1','0','127.0.0.1:10001');"
$MYSQL "insert into carrierroute (id, carrier, domain, scan_prefix, flags, mask, prob, strip, rewrite_host)
values ('5','3','10','49','1','3','1','0','127.0.0.1:10000');"
$MYSQL "insert into carrierroute (id, carrier, domain, scan_prefix, flags, mask, prob, strip, rewrite_host)
values ('6','3','10','49','2','3','1','0','127.0.0.1:10002');"
$MYSQL "insert into carrierroute (id, carrier, domain, scan_prefix, flags, mask, prob, strip, rewrite_host)
values ('7','3','1','49','0','0','1','0','127.0.0.1:10000');"
$MYSQL "insert into carrierroute (id, carrier, domain, scan_prefix, flags, mask, prob, strip, rewrite_host)
values ('8','3','2','49','0','0','1','0','127.0.0.1:10000');"

$MYSQL "insert into carrierfailureroute(id, carrier, domain, scan_prefix, host_name, reply_code,
flags, mask, next_domain) values ('1', '3', '10', '49', '127.0.0.1:10000', '5..', '0', '0', '1');"
$MYSQL "insert into carrierfailureroute(id, carrier, domain, scan_prefix, host_name, reply_code,
flags, mask, next_domain) values ('2', '3', '1', '49', '127.0.0.1:10000', '483', '0', '0', '2');"
$MYSQL "insert into carrierfailureroute(id, carrier, domain, scan_prefix, host_name, reply_code,
flags, mask, next_domain) values ('3', '3', '1', '49', '127.0.0.1:9000', '4..', '0', '0', '2');"

$MYSQL "alter table subscriber add cr_preferred_carrier int(10) default NULL;"

$MYSQL "insert into subscriber (username, cr_preferred_carrier) values ('49721123456786', 2);"
$MYSQL "insert into subscriber (username, cr_preferred_carrier) values ('49721123456785', 3);"


$BIN -L $MOD_DIR -Y $RUN_DIR -P $PIDFILE -w . -f $CFG > /dev/null

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
sleep 1

if [ "$ret" -eq 0 ] ; then
	sipp -sn uas -bg -i localhost -m 10 -p 10000 &> /dev/null
	sipp -sn uac -s 49721123456785 127.0.0.1:5060 -i 127.0.0.1 -m 10 -p 5061 &> /dev/null
	ret=$?
fi;
sleep 2

if [ "$ret" -eq 0 ] ; then
	killall sipp &> /dev/null
	sipp -sf failure_route.xml -bg -i localhost -m 1 -p 10000 &> /dev/null
	sipp -sn uac -s 49721123456785 127.0.0.1:5060 -i 127.0.0.1 -m 1 -p 5061 #&> /dev/null
	ret=$?
fi;

$MYSQL "insert into carrierfailureroute(id, carrier, domain, scan_prefix, host_name, reply_code,
flags, mask, next_domain) values ('4', '3', '1', '49', '127.0.0.1:10000', '4..', '0', '0', '4');"
$MYSQL "insert into carrierfailureroute(id, carrier, domain, scan_prefix, host_name, reply_code,
flags, mask, next_domain) values ('5', '3', '1', '49', '127.0.0.1:10000', '486', '0', '0', '2');"

if [ ! "$ret" -eq 0 ] ; then
	$CTL fifo cr_reload_routes
	killall sipp &> /dev/null
	sipp -sf failure_route.xml -bg -i localhost -m 10 -p 10000 &> /dev/null
	sipp -sn uac -s 49721123456785 127.0.0.1:5060 -i 127.0.0.1 -m 10 -p 5061 &> /dev/null
	ret=$?
fi;


kill_kamailio
killall -9 sipp

# cleanup database
$MYSQL "delete from carrier_name where id = 1;"
$MYSQL "delete from carrier_name where id = 2;"
$MYSQL "delete from carrier_name where id = 3;"
$MYSQL "delete from domain_name where id = 10;"
$MYSQL "delete from domain_name where id = 1;"
$MYSQL "delete from domain_name where id = 2;"
$MYSQL "delete from carrierroute where carrier=1;"
$MYSQL "delete from carrierroute where carrier=2;"
$MYSQL "delete from carrierroute where carrier=3;"
$MYSQL "delete from carrierfailureroute where carrier=1;"
$MYSQL "delete from carrierfailureroute where carrier=2;"
$MYSQL "delete from carrierfailureroute where carrier=3;"

$MYSQL "delete from subscriber where username='49721123456786';"
$MYSQL "delete from subscriber where username='49721123456785';"
$MYSQL "alter table subscriber drop cr_preferred_carrier;"

mv $CFG.bak $CFG

exit $ret
