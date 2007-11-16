#!/bin/bash
# loads a carrierroute config for loadbalancing from database

CFG=14.cfg

if [ ! -e ../modules/carrierroute/carrierroute.so ] ; then
	echo "carrierroute not found, not run"
	exit 0
fi ;

cp $CFG $CFG.bak

# setup config
echo "loadmodule \"mysql/mysql.so\"" >> $CFG
echo "modparam(\"carrierroute\", \"config_source\", \"db\")" >> $CFG

# setup database
MYSQL="mysql openser -u openser --password=openserrw -e"

$MYSQL "insert into openser.route_tree (id, carrier) values ('1', 'carrier1');"
$MYSQL "insert into openser.route_tree (id, carrier) values ('2', 'default');"

$MYSQL "insert into openser.carrierroute (id, carrier, scan_prefix, domain, prob, strip, rewrite_host) values ('1','1','49','0','0.5','0','host1.local');"
$MYSQL "insert into openser.carrierroute (id, carrier, scan_prefix, domain, prob, strip, rewrite_host) values ('2','1','49','0','0.5','0','host2.local');"
$MYSQL "insert into openser.carrierroute (id, carrier, scan_prefix, domain, prob, strip, rewrite_host) values ('3','1','42','0','0.3','0','host3.local');"
$MYSQL "insert into openser.carrierroute (id, carrier, scan_prefix, domain, prob, strip, rewrite_host) values ('4','1','42','0','0.7','0','host4.local');"
$MYSQL "insert into openser.carrierroute (id, carrier, scan_prefix, domain, prob, strip, rewrite_host) values ('5','1','','0','0.1','0','host5.local');"
$MYSQL "insert into openser.carrierroute (id, carrier, scan_prefix, domain, prob, strip, rewrite_host) values ('6','2','','0','1','0','host6.local');"

../openser -f $CFG > /dev/null
ret=$?

sleep 1

cd ../scripts

TMPFILE=`mktemp -t openser-test.XXXXXXXXXX`

if [ "$ret" -eq 0 ] ; then
	./openserctl fifo cr_dump_routes > $TMPFILE
	ret=$?
fi ;

if [ "$ret" -eq 0 ] ; then
	tmp=`grep -v "Printing routing information:
Printing tree for carrier carrier1 (1)
Printing tree for domain 0
        42: 70.140 %, 'host4.local': ON, '0', '', '', ''
        42: 30.060 %, 'host3.local': ON, '0', '', '', ''
        49: 50.000 %, 'host2.local': ON, '0', '', '', ''
        49: 50.000 %, 'host1.local': ON, '0', '', '', ''
      NULL: 100.000 %, 'host5.local': ON, '0', '', '', ''
Printing tree for carrier default (2)
Printing tree for domain 0
      NULL: 100.000 %, 'host6.local': ON, '0', '', '', ''" $TMPFILE`
	if [ "$tmp" = "" ] ; then
		ret=0
	else
		ret=1
	fi ;
fi ;

killall -9 openser

# cleanup database
$MYSQL "delete from route_tree where id = 1;"
$MYSQL "delete from route_tree where id = 2;"
$MYSQL "delete from openser.carrierroute where carrier=1;"
$MYSQL "delete from openser.carrierroute where carrier=2;"

cd ../test

mv $CFG.bak $CFG
rm $TMPFILE

exit $ret