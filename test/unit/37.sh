#!/bin/bash
# loading times for bigger carrierroute config from mysql database

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

CFG=13.cfg
# number of routes, multiplied with 10
# if you want to increase this above 4500, comment the fifo command below,
# otherwise this will fails with memory allocation errors (with 1MB PKG mem)
NR=450

if ! (check_kamailio && check_module "carrierroute" && check_module "db_mysql" && check_mysql); then
	exit 0
fi ;

cp $CFG $CFG.bak

# setup config
echo "loadmodule \"db_mysql/db_mysql.so\"" >> $CFG
echo "modparam(\"carrierroute\", \"config_source\", \"db\")" >> $CFG

# setup database
$MYSQL "insert into carrier_name (id, carrier) values ('1', 'default');"
$MYSQL "insert into carrier_name (id, carrier) values ('2', 'carrier2');"
$MYSQL "insert into carrier_name (id, carrier) values ('3', 'carrier3');"
$MYSQL "insert into carrier_name (id, carrier) values ('4', 'carrier4');"
$MYSQL "insert into carrier_name (id, carrier) values ('5', 'carrier5');"
$MYSQL "insert into carrier_name (id, carrier) values ('6', 'carrier6');"
$MYSQL "insert into carrier_name (id, carrier) values ('7', 'carrier7');"
$MYSQL "insert into carrier_name (id, carrier) values ('8', 'carrier8');"
$MYSQL "insert into carrier_name (id, carrier) values ('9', 'carrier9');"
$MYSQL "insert into carrier_name (id, carrier) values ('10', 'carrier10');"
$MYSQL "insert into carrier_name (id, carrier) values ('11', 'carrier11');"


COUNTER=0
while [  $COUNTER -lt $NR ]; do
	COUNTER=$(($COUNTER+1))
	domain=`expr $COUNTER % 9`
	$MYSQL "insert into domain_name (id, domain) values ('$COUNTER', '$domain');"
	$MYSQL "insert into carrierroute (carrier, domain, scan_prefix, prob, strip, rewrite_host) values ('2','$COUNTER','$RANDOM','0.5','0','host-$RANDOM'); insert into carrierroute (carrier, domain, scan_prefix, prob, strip, rewrite_host) values ('3','$COUNTER','$RANDOM','0.5','0','host-$RANDOM'); insert into carrierroute (carrier, domain, scan_prefix, prob, strip, rewrite_host) values ('4','$COUNTER','$RANDOM','0.5','0','host-$RANDOM'); insert into carrierroute (carrier, domain, scan_prefix, prob, strip, rewrite_host) values ('5','$COUNTER','$RANDOM','0.5','0','host-$RANDOM'); insert into carrierroute (carrier, domain, scan_prefix, prob, strip, rewrite_host) values ('6','$COUNTER','$RANDOM','0.5','0','host-$RANDOM'); insert into carrierroute (carrier, domain, scan_prefix, prob, strip, rewrite_host) values ('7','$COUNTER','$RANDOM','0.5','0','host-$RANDOM'); insert into carrierroute (carrier, domain, scan_prefix, prob, strip, rewrite_host) values ('8','$COUNTER','$RANDOM','0.5','0','host-$RANDOM'); insert into carrierroute (carrier, domain, scan_prefix, prob, strip, rewrite_host) values ('9','$COUNTER','$RANDOM','0.5','0','host-$RANDOM'); insert into carrierroute (carrier, domain, scan_prefix, prob, strip, rewrite_host) values ('10','$COUNTER','$RANDOM','0.5','0','host-$RANDOM'); insert into carrierroute (carrier, domain, scan_prefix, prob, strip, rewrite_host) values ('11','$COUNTER','$RANDOM','0.5','0','host-$RANDOM');"

done

$BIN -L $MOD_DIR -Y $RUN_DIR -P $PIDFILE -m 128 -w . -f $CFG > /dev/null
ret=$?

# adjust if you have bigger rule sets
sleep 20

if [ $ret -eq 0 ] ; then
	tmp=`$CTL fifo cr_dump_routes | grep "host-" | wc -l`
	let "TMPNR = $NR * 10"
	if ! [ $tmp -eq $TMPNR ]; then
		ret=1
	fi;
fi ;

kill_kamailio

# cleanup database
$MYSQL "delete from carrier_name where id = 1;"
$MYSQL "delete from carrier_name where id = 2;"
$MYSQL "delete from carrier_name where id = 3;"
$MYSQL "delete from carrier_name where id = 4;"
$MYSQL "delete from carrier_name where id = 5;"
$MYSQL "delete from carrier_name where id = 6;"
$MYSQL "delete from carrier_name where id = 7;"
$MYSQL "delete from carrier_name where id = 8;"
$MYSQL "delete from carrier_name where id = 9;"
$MYSQL "delete from carrier_name where id = 10;"
$MYSQL "delete from carrier_name where id = 11;"

COUNTER=0
while [  $COUNTER -lt $NR ]; do
	COUNTER=$(($COUNTER+1))
	$MYSQL "delete from domain_name where id = $COUNTER;"
done

$MYSQL "delete from carrierroute where carrier=1;"
$MYSQL "delete from carrierroute where carrier=2;"
$MYSQL "delete from carrierroute where carrier=3;"
$MYSQL "delete from carrierroute where carrier=4;"
$MYSQL "delete from carrierroute where carrier=5;"
$MYSQL "delete from carrierroute where carrier=6;"
$MYSQL "delete from carrierroute where carrier=7;"
$MYSQL "delete from carrierroute where carrier=8;"
$MYSQL "delete from carrierroute where carrier=9;"
$MYSQL "delete from carrierroute where carrier=10;"
$MYSQL "delete from carrierroute where carrier=11;"

mv $CFG.bak $CFG

exit $ret
