#!/bin/bash
# loads a carrierroute config for loadbalancing from postgres database

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

if ! (check_kamailio && check_module "carrierroute" && check_module "db_postgres" && check_postgres); then
	exit 0
fi ;

CFG=13.cfg

cp $CFG $CFG.bak

# setup config
echo "loadmodule \"db_postgres/db_postgres.so\"" >> $CFG
echo "modparam(\"carrierroute\", \"config_source\", \"db\")" >> $CFG
echo "modparam(\"carrierroute\", \"db_url\", \"postgres://kamailioro:kamailioro@localhost/kamailio\")" >> $CFG

# setup database
$PSQL "insert into carrier_name (id, carrier) values ('1', 'carrier1');
insert into carrier_name (id, carrier) values ('2', 'default');
insert into carrier_name (id, carrier) values ('3', 'premium');
insert into domain_name (id, domain) values ('10', 'domain1');
insert into carrierroute (id, carrier, scan_prefix, domain, prob, strip, rewrite_host) values ('1','1','49','10','0.5','0','host1.local.domain');
insert into carrierroute (id, carrier, scan_prefix, domain, prob, strip, rewrite_host) values ('2','1','49','10','0.5','0','host2.local.domain');
insert into carrierroute (id, carrier, scan_prefix, domain, prob, strip, rewrite_host) values ('3','1','42','10','0.3','0','host3.local');
insert into carrierroute (id, carrier, scan_prefix, domain, prob, strip, rewrite_host) values ('4','1','42','10','0.7','0','host4.local');
insert into carrierroute (id, carrier, scan_prefix, domain, prob, strip, rewrite_host) values ('5','1','1','10','0.5','0','host1-ca.local:5060');
insert into carrierroute (id, carrier, scan_prefix, domain, prob, strip, rewrite_host) values ('6','1','1','10','0.5','0','host2-ca.local.domain:5060');
insert into carrierroute (id, carrier, scan_prefix, domain, prob, strip, rewrite_host) values ('10','1','','10','0.1','0','host5.local');
insert into carrierroute (id, carrier, scan_prefix, domain, prob, strip, rewrite_host) values ('20','2','','10','1','0','host6');
insert into carrierroute (id, carrier, scan_prefix, domain, prob, strip, rewrite_host) values ('21','3','','10','1','0','premium.host.local');"

$BIN -L $MOD_DIR -Y $RUN_DIR -P $PIDFILE -w . -f $CFG > /dev/null
ret=$?

sleep 1

TMPFILE=`mktemp -t kamailio-test.XXXXXXXXXX`

if [ "$ret" -eq 0 ] ; then
	$CTL fifo cr_dump_routes > $TMPFILE
	ret=$?
fi ;

if [ "$ret" -eq 0 ] ; then
	tmp=`grep -v "Printing routing information:
Printing tree for carrier 'carrier1' (1)
Printing tree for domain 'domain1' (10)
         1: 50.000 %, 'host2-ca.local.domain:5060': ON, '0', '', '', ''
         1: 50.000 %, 'host1-ca.local:5060': ON, '0', '', '', ''
        42: 70.140 %, 'host4.local': ON, '0', '', '', ''
        42: 30.060 %, 'host3.local': ON, '0', '', '', ''
        49: 50.000 %, 'host2.local.domain': ON, '0', '', '', ''
        49: 50.000 %, 'host1.local.domain': ON, '0', '', '', ''
      NULL: 100.000 %, 'host5.local': ON, '0', '', '', ''
Printing tree for carrier 'default' (2)
Printing tree for domain 'domain1' (10)
      NULL: 100.000 %, 'host6': ON, '0', '', '', ''
Printing tree for carrier 'premium' (3)
Printing tree for domain 'domain1' (10)
      NULL: 100.000 %, 'premium.host.local': ON, '0', '', '', ''" $TMPFILE`
	if [ "$tmp" = "" ] ; then
		ret=0
	else
		ret=1
	fi ;
fi ;

kill_kamailio

# cleanup database
$PSQL "delete from carrier_name where id = 1;
delete from carrier_name where id = 2;
delete from carrier_name where id = 3;
delete from domain_name where id = 10;
delete from carrierroute where carrier=1;
delete from carrierroute where carrier=2;
delete from carrierroute where carrier=3;"

mv $CFG.bak $CFG
rm $TMPFILE

exit $ret
