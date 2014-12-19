#!/bin/bash
#
# build script for travis CI
# environment based on Ubuntu 12.04 LTS (precise)
#
export JAVA_HOME="/usr/lib/jvm/java-gcj"
EXCLUDED_MODULES=""
EXTRA_EXCLUDED_MODULES="bdb dbtext oracle pa iptrtpproxy mi_xmlrpc dnssec kazoo cnxcc"
PACKAGE_GROUPS="mysql postgres berkeley unixodbc radius presence ldap xml perl utils lua memcached \
	snmpstats carrierroute xmpp cpl redis python geoip\
	sqlite json mono ims sctp java \
	purple tls outbound websocket autheph"
echo "make cfg"
make FLAVOUR=kamailio cfg \
	skip_modules="${EXCLUDED_MODULES} ${EXTRA_EXCLUDED_MODULES}" \
	group_include="kstandard"
echo "make all"
make all
echo "make groups"
for grp in ${PACKAGE_GROUPS}; do
	make every-module group_include="k${grp}"
done
