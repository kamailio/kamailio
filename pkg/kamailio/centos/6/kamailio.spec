%define name	kamailio
%define ver	4.3.2
%define rel	0.0%{dist}



Summary:	Kamailio (former OpenSER) - the Open Source SIP Server
Name:		%name
Version:	%ver
Release:	%rel
Packager:	Peter Dunkley <peter@dunkley.me.uk>
License:	GPL
Group:		System Environment/Daemons
Source:		http://kamailio.org/pub/kamailio/%{ver}/src/%{name}-%{ver}_src.tar.gz
URL:		http://kamailio.org/
Vendor:		kamailio.org
BuildRoot:	%{_tmppath}/%{name}-%{ver}-buildroot
Conflicts:	kamailio-auth-ephemeral < %ver, kamailio-bdb < %ver
Conflicts:	kamailio-carrierroute < %ver, kamailio-cpl < %ver
Conflicts:	kamailio-dialplan < %ver, kamailio-dnssec < %ver
Conflicts:	kamailio-geoip < %ver, kamailio-gzcompress < %ver
Conflicts:	kamailio-ims < %ver, kamailio-java < %ver, kamailio-json < %ver
Conflicts:	kamailio-lcr < %ver, kamailio-ldap < %ver, kamailio-lua < %ver
Conflicts:	kamailio-memcached < %ver, kamailio-mysql < %ver
Conflicts:	kamailio-outbound < %ver, kamailio-perl < %ver
Conflicts:	kamailio-postgresql < %ver, kamailio-presence < %ver
Conflicts:	kamailio-purple < %ver, kamailio-python < %ver
Conflicts:	kamailio-radius < % ver, kamailio-redis < %ver
Conflicts:	kamailio-regex < %ver, kamailio-sctp < %ver
Conflicts:	kamailio-snmpstats < %ver, kamailio-sqlite < %ver
Conflicts:	kamailio-tls < %ver, kamailio-unixodbc < %ver
Conflicts:	kamailio-utils < %ver, kamailio-websocket < %ver
Conflicts:	kamailio-xhttp-pi < %ver, kamailio-xmlops < %ver
Conflicts:	kamailio-xmlrpc < %ver, kamailio-xmpp < %ver
BuildRequires:	bison, flex, gcc, make, redhat-rpm-config

%description
Kamailio (former OpenSER) is an Open Source SIP Server released under GPL, able
to handle thousands of call setups per second. Among features: asynchronous TCP,
UDP and SCTP, secure communication via TLS for VoIP (voice, video); IPv4 and
IPv6; SIMPLE instant messaging and presence with embedded XCAP server and MSRP
relay; ENUM; DID and least cost routing; load balancing; routing fail-over;
accounting, authentication and authorization; support for many backend systems
such as MySQL, Postgres, Oracle, Radius, LDAP, Redis, Cassandra; XMLRPC control
interface, SNMP monitoring. It can be used to build large VoIP servicing
platforms or to scale up SIP-to-PSTN gateways, PBX systems or media servers
like Asterisk™, FreeSWITCH™ or SEMS.


%package	auth-ephemeral
Summary:	Functions for authentication using ephemeral credentials.
Group:		System Environment/Daemons
Requires:	openssl, kamailio = %ver
BuildRequires:	openssl-devel

%description auth-ephemeral
Functions for authentication using ephemeral credentials.


%package	bdb
Summary:	Berkeley database connectivity for Kamailio.
Group:		System Environment/Daemons
Requires:	db4, kamailio = %ver
BuildRequires:	db4-devel

%description	bdb
Berkeley database connectivity for Kamailio.


%package	carrierroute
Summary:	The carrierroute module for Kamailio.
Group:		System Environment/Daemons
Requires:	epel-release, libconfuse, kamailio = %ver
BuildRequires:	epel-release, libconfuse-devel

%description	carrierroute
The carrierroute module for Kamailio.


%package	cpl
Summary:	CPL (Call Processing Language) interpreter for Kamailio.
Group:		System Environment/Daemons
Requires:	libxml2, kamailio = %ver
BuildRequires:	libxml2-devel

%description	cpl
CPL (Call Processing Language) interpreter for Kamailio.


%package	dialplan
Summary:	String translations based on rules for Kamailio.
Group:		System Environment/Daemons
Requires:	pcre, kamailio = %ver
BuildRequires:	pcre-devel

%description	dialplan
String translations based on rules for Kamailio.


%package	dnssec
Summary:	DNSSEC support for Kamailio.
Group:		System Environment/Daemons
Requires:	epel-release, dnssec-tools-libs, kamailio = %ver
BuildRequires:	epel-release, dnssec-tools-libs-devel

%description	dnssec
DNSSEC support for Kamailio.


%package	geoip
Summary:	MaxMind GeoIP support for Kamailio.
Group:		System Environment/Daemons
Requires:	epel-release, GeoIP, kamailio = %ver
BuildRequires:	epel-release, GeoIP-devel

%description	geoip
MaxMind GeoIP support for Kamailio.


%package	gzcompress
Summary:	Compressed body (SIP and HTTP) handling for kamailio.
Group:		System Environment/Daemons
Requires:	zlib, kamailio = %ver
BuildRequires:	zlib-devel

%description	gzcompress
Compressed body (SIP and HTTP) handling for kamailio.


%package	ims
Summary:	IMS modules and extensions module for Kamailio.
Group:		System Environment/Daemons
Requires:	libxml2, kamailio = %ver
BuildRequires:	libxml2-devel

%description	ims
IMS modules and extensions module for Kamailio.


%package	java
Summary:	Java extensions for Kamailio.
Group:		System Environment/Daemons
Requires:	libgcj, java-1.6.0-openjdk, kamailio = %ver
BuildRequires:	libgcj-devel, java-1.6.0-openjdk-devel, ant

%description	java
Java extensions for Kamailio.


%package	json
Summary:	json string handling and RPC modules for Kamailio.
Group:		System Environment/Daemons
Requires:	epel-release, json-c, libevent, kamailio = %ver
BuildRequires:	epel-release, json-c-devel, libevent-devel

%description	json
json string handling and RPC modules for Kamailio.


%package	lcr
Summary:	Least cost routing for Kamailio.
Group:		System Environment/Daemons
Requires:	pcre, kamailio = %ver
BuildRequires:	pcre-devel

%description	lcr
Least cost routing for Kamailio.


%package	ldap
Summary:	LDAP search interface for Kamailio.
Group:		System Environment/Daemons
Requires:	openldap, kamailio = %ver
BuildRequires:	openldap-devel

%description	ldap
LDAP search interface for Kamailio.


%package	lua
Summary:	Lua extensions for Kamailio.
Group:		System Environment/Daemons
Requires:	kamailio = %ver
BuildRequires:	lua-devel

%description	lua
Lua extensions for Kamailio.


%package	memcached
Summary:	memcached configuration file support for Kamailio.
Group:		System Environment/Daemons
Requires:	libmemcached, kamailio = %ver
BuildRequires:	libmemcached-devel

%description	memcached
memcached configuration file support for Kamailio.


%package	mysql
Summary:	MySQL database connectivity for Kamailio.
Group:		System Environment/Daemons
Requires:	mysql-libs, kamailio = %ver
BuildRequires:	mysql-devel zlib-devel

%description	mysql
MySQL database connectivity for Kamailio.


%package	outbound
Summary:	Outbound (RFC 5626) support for Kamailio.
Group:		System Environment/Daemons
Requires:	openssl, kamailio = %ver
BuildRequires:	openssl-devel

%description	outbound
RFC 5626, "Managing Client-Initiated Connections in the Session Initiation
Protocol (SIP)" support for Kamailio.


%package	perl
Summary:	Perl extensions and database driver for Kamailio.
Group:		System Environment/Daemons 
Requires:	mod_perl, kamailio = %ver
BuildRequires:	mod_perl-devel

%description	perl
Perl extensions and database driver for Kamailio.


%package	postgresql
Summary:	PostgreSQL database connectivity for Kamailio.
Group:		System Environment/Daemons
Requires:	postgresql-libs, kamailio = %ver
BuildRequires:	postgresql-devel

%description	postgresql
PostgreSQL database connectivity for Kamailio.


%package	presence
Summary:	SIP Presence (and RLS, XCAP, etc) support for Kamailio.
Group:		System Environment/Daemons
Requires:	libxml2, libcurl, kamailio = %ver, kamailio-xmpp = %ver
BuildRequires:	libxml2-devel, libcurl-devel

%description	presence
SIP Presence (and RLS, XCAP, etc) support for Kamailio.


%package	purple
Summary:	Multi-protocol IM and presence gateway module.
Group:		System Environment/Daemons
Requires:	glib2, libpurple, libxml2, kamailio = %ver
Requires:	kamailio-presence = %ver
BuildRequires:	glib2-devel, libpurple-devel, libxml2-devel

%description	purple
Multi-protocol IM and presence gateway module.


%package	python
Summary:	Python extensions for Kamailio.
Group:		System Environment/Daemons
Requires:	python, kamailio = %ver
BuildRequires:	python-devel

%description	python
Python extensions for Kamailio.


%package	radius
Summary:	RADIUS modules for Kamailio.
Group:		System Environment/Daemons
Requires:	epel-release, radiusclient-ng, kamailio = %ver
BuildRequires:	epel-release, radiusclient-ng-devel

%description	radius
RADIUS modules for Kamailio.


%package	redis
Summary:	Redis configuration file support for Kamailio.
Group:		System Environment/Daemons
Requires:	epel-release, hiredis, kamailio = %ver
BuildRequires:	epel-release, hiredis-devel

%description	redis
Redis configuration file support for Kamailio.


%package	regex
Summary:	PCRE mtaching operations for Kamailio.
Group:		System Environment/Daemons
Requires:	pcre, kamailio = %ver
BuildRequires:	pcre-devel

%description	regex
PCRE mtaching operations for Kamailio.


%package	sctp
Summary:	SCTP transport for Kamailio.
Group:		System Environment/Daemons
Requires:	lksctp-tools, kamailio = %ver
BuildRequires:	lksctp-tools-devel

%description	sctp
SCTP transport for Kamailio.


%package	snmpstats
Summary:	SNMP management interface (scalar statistics) for Kamailio.
Group:		System Environment/Daemons
Requires:	net-snmp-libs, kamailio = %ver
BuildRequires:	net-snmp-devel

%description	snmpstats
SNMP management interface (scalar statistics) for Kamailio.


%package	sqlite
Summary:	SQLite database connectivity for Kamailio.
Group:		System Environment/Daemons
Requires:	sqlite, kamailio = %ver
BuildRequires:	sqlite-devel

%description	sqlite
SQLite database connectivity for Kamailio.


%package	tls
Summary:	TLS transport for Kamailio.
Group:		System Environment/Daemons
Requires:	openssl, kamailio = %ver
BuildRequires:	openssl-devel

%description	tls
TLS transport for Kamailio.


%package	unixodbc
Summary:	unixODBC database connectivity for Kamailio.
Group:		System Environment/Daemons
Requires:	unixODBC, kamailio = %ver
BuildRequires:	unixODBC-devel

%description	unixodbc
unixODBC database connectivity for Kamailio.


%package	utils
Summary:	Non-SIP utitility functions for Kamailio.
Group:		System Environment/Daemons
Requires:	libcurl, libxml2, kamailio = %ver
BuildRequires:	libcurl-devel, libxml2-devel

%description	utils
Non-SIP utitility functions for Kamailio.


%package	websocket
Summary:	WebSocket transport for Kamailio.
Group:		System Environment/Daemons
Requires:	libunistring, openssl, kamailio = %ver
BuildRequires:	libunistring-devel, openssl-devel

%description	websocket
WebSocket transport for Kamailio.


%package	xhttp-pi
Summary:	Web-provisioning interface for Kamailio.
Group:		System Environment/Daemons
Requires:	libxml2, kamailio = %ver
BuildRequires:	libxml2-devel

%description	xhttp-pi
Web-provisioning interface for Kamailio.


%package	xmlops
Summary:	XML operation functions for Kamailio.
Group:		System Environment/Daemons
Requires:	libxml2, kamailio = %ver
BuildRequires:	libxml2-devel

%description	xmlops
XML operation functions for Kamailio.


%package	xmlrpc
Summary:	XMLRPC transport and encoding for Kamailio RPCs and MI commands.
Group:		System Environment/Daemons
Requires:	libxml2, xmlrpc-c, kamailio = %ver
BuildRequires:	libxml2-devel, xmlrpc-c-devel

%description	xmlrpc
XMLRPC transport and encoding for Kamailio RPCs and MI commands.


%package	xmpp
Summary:	SIP/XMPP IM gateway for Kamailio.
Group:		System Environment/Daemons
Requires:	expat, kamailio = %ver
BuildRequires:	expat-devel

%description	xmpp
SIP/XMPP IM gateway for Kamailio.



%prep
%setup -n %{name}-%{ver}



%build
make cfg prefix=/usr cfg_prefix=$RPM_BUILD_ROOT basedir=$RPM_BUILD_ROOT \
	cfg_target=/%{_sysconfdir}/kamailio/ modules_dirs="modules"
make
make every-module skip_modules="app_mono db_cassandra db_oracle iptrtpproxy \
	jabber ndb_cassandra osp" \
	group_include="kstandard kautheph kberkeley kcarrierroute kcpl \
	kdnssec kgeoip kgzcompress kims kjava kjson kldap klua kmemcached \
	kmi_xmlrpc kmysql koutbound kperl kpostgres kpresence kpurple kpython \
	kradius kredis ksctp ksnmpstats ksqlite ktls kunixodbc kutils \
	kwebsocket kxml kxmpp"
cd modules/app_java/kamailio_java_folder/java
ant
cd ../../../..
make utils



%install
[ "$RPM_BUILD_ROOT" != "/" ] && rm -rf "$RPM_BUILD_ROOT"

make install
make install-modules-all skip_modules="app_mono db_cassandra db_oracle \
	iptrtpproxy jabber osp" \
	group_include="kstandard kautheph kberkeley kcarrierroute kcpl \
	kdnssec kgeoip kgzcompress kims kjava kjson kldap klua kmemcached \
	kmi_xmlrpc kmysql koutbound kperl kpostgres kpresence kpurple kpython \
	kradius kredis ksctp ksnmpstats ksqlite ktls kunixodbc kutils \
	kwebsocket kxml kxmpp"

mkdir -p $RPM_BUILD_ROOT/%{_libdir}/kamailio/java
install -m644 modules/app_java/kamailio_java_folder/java/Kamailio.class \
	$RPM_BUILD_ROOT/%{_libdir}/kamailio/java
install -m644 modules/app_java/kamailio_java_folder/java/kamailio.jar \
	$RPM_BUILD_ROOT/%{_libdir}/kamailio/java

mkdir -p $RPM_BUILD_ROOT/%{_sysconfdir}/rc.d/init.d
install -m755 pkg/kamailio/centos/%{?centos}/kamailio.init \
		$RPM_BUILD_ROOT/%{_sysconfdir}/rc.d/init.d/kamailio

mkdir -p $RPM_BUILD_ROOT/%{_sysconfdir}/sysconfig
install -m644 pkg/kamailio/centos/%{?centos}/kamailio.sysconfig \
		$RPM_BUILD_ROOT/%{_sysconfdir}/sysconfig/kamailio



%pre
/usr/sbin/groupadd -r kamailio 2> /dev/null || :
/usr/sbin/useradd -r -g kamailio -s /bin/false -c "Kamailio daemon" -d \
		%{_libdir}/kamailio kamailio 2> /dev/null || :



%clean
rm -rf "$RPM_BUILD_ROOT"



%post
/sbin/chkconfig --add kamailio



%preun
if [ $1 = 0 ]; then
	/sbin/service kamailio stop > /dev/null 2>&1
	/sbin/chkconfig --del kamailio
fi



%files
%defattr(-,root,root)
%dir %{_docdir}/kamailio
%doc %{_docdir}/kamailio/AUTHORS
%doc %{_docdir}/kamailio/NEWS
%doc %{_docdir}/kamailio/INSTALL
%doc %{_docdir}/kamailio/README
%doc %{_docdir}/kamailio/README-MODULES

%dir %{_docdir}/kamailio/modules
%doc %{_docdir}/kamailio/modules/README.acc
%doc %{_docdir}/kamailio/modules/README.alias_db
%doc %{_docdir}/kamailio/modules/README.async
%doc %{_docdir}/kamailio/modules/README.auth
%doc %{_docdir}/kamailio/modules/README.auth_db
%doc %{_docdir}/kamailio/modules/README.auth_diameter
%doc %{_docdir}/kamailio/modules/README.avp
%doc %{_docdir}/kamailio/modules/README.avpops
%doc %{_docdir}/kamailio/modules/README.benchmark
%doc %{_docdir}/kamailio/modules/README.blst
%doc %{_docdir}/kamailio/modules/README.call_control
%doc %{_docdir}/kamailio/modules/README.cfg_db
%doc %{_docdir}/kamailio/modules/README.cfg_rpc
%doc %{_docdir}/kamailio/modules/README.cfgutils
%doc %{_docdir}/kamailio/modules/README.cnxcc
%doc %{_docdir}/kamailio/modules/README.corex
%doc %{_docdir}/kamailio/modules/README.counters
%doc %{_docdir}/kamailio/modules/README.ctl
%doc %{_docdir}/kamailio/modules/README.db_cluster
%doc %{_docdir}/kamailio/modules/README.db_flatstore
%doc %{_docdir}/kamailio/modules/README.db_text
%doc %{_docdir}/kamailio/modules/README.db2_ops
%doc %{_docdir}/kamailio/modules/README.debugger
%doc %{_docdir}/kamailio/modules/README.dialog
%doc %{_docdir}/kamailio/modules/README.dialog_ng
%doc %{_docdir}/kamailio/modules/README.dispatcher
%doc %{_docdir}/kamailio/modules/README.diversion
%doc %{_docdir}/kamailio/modules/README.dmq
%doc %{_docdir}/kamailio/modules/README.domain
%doc %{_docdir}/kamailio/modules/README.domainpolicy
%doc %{_docdir}/kamailio/modules/README.drouting
%doc %{_docdir}/kamailio/modules/README.enum
%doc %{_docdir}/kamailio/modules/README.exec
%doc %{_docdir}/kamailio/modules/README.group
%doc %{_docdir}/kamailio/modules/README.htable
%doc %{_docdir}/kamailio/modules/README.imc
%doc %{_docdir}/kamailio/modules/README.ipops
%doc %{_docdir}/kamailio/modules/README.kex
%doc %{_docdir}/kamailio/modules/README.malloc_test
%doc %{_docdir}/kamailio/modules/README.mangler
%doc %{_docdir}/kamailio/modules/README.matrix
%doc %{_docdir}/kamailio/modules/README.maxfwd
%doc %{_docdir}/kamailio/modules/README.mediaproxy
%doc %{_docdir}/kamailio/modules/README.mi_datagram
%doc %{_docdir}/kamailio/modules/README.mi_fifo
%doc %{_docdir}/kamailio/modules/README.mi_rpc
%doc %{_docdir}/kamailio/modules/README.mohqueue
%doc %{_docdir}/kamailio/modules/README.mqueue
%doc %{_docdir}/kamailio/modules/README.msilo
%doc %{_docdir}/kamailio/modules/README.msrp
%doc %{_docdir}/kamailio/modules/README.mtree
%doc %{_docdir}/kamailio/modules/README.nat_traversal
%doc %{_docdir}/kamailio/modules/README.nathelper
%doc %{_docdir}/kamailio/modules/README.p_usrloc
%doc %{_docdir}/kamailio/modules/README.path
%doc %{_docdir}/kamailio/modules/README.pdb
%doc %{_docdir}/kamailio/modules/README.pdt
%doc %{_docdir}/kamailio/modules/README.permissions
%doc %{_docdir}/kamailio/modules/README.pike
%doc %{_docdir}/kamailio/modules/README.pipelimit
%doc %{_docdir}/kamailio/modules/README.prefix_route
%doc %{_docdir}/kamailio/modules/README.print
%doc %{_docdir}/kamailio/modules/README.print_lib
%doc %{_docdir}/kamailio/modules/README.pv
%doc %{_docdir}/kamailio/modules/README.qos
%doc %{_docdir}/kamailio/modules/README.ratelimit
%doc %{_docdir}/kamailio/modules/README.registrar
%doc %{_docdir}/kamailio/modules/README.rr
%doc %{_docdir}/kamailio/modules/README.rtimer
%doc %{_docdir}/kamailio/modules/README.rtpproxy
%doc %{_docdir}/kamailio/modules/README.rtpengine
%doc %{_docdir}/kamailio/modules/README.sanity
%doc %{_docdir}/kamailio/modules/README.sca
%doc %{_docdir}/kamailio/modules/README.sdpops
%doc %{_docdir}/kamailio/modules/README.seas
%doc %{_docdir}/kamailio/modules/README.sipcapture
%doc %{_docdir}/kamailio/modules/README.sipt
%doc %{_docdir}/kamailio/modules/README.siptrace
%doc %{_docdir}/kamailio/modules/README.siputils
%doc %{_docdir}/kamailio/modules/README.sl
%doc %{_docdir}/kamailio/modules/README.sms
%doc %{_docdir}/kamailio/modules/README.speeddial
%doc %{_docdir}/kamailio/modules/README.sqlops
%doc %{_docdir}/kamailio/modules/README.sst
%doc %{_docdir}/kamailio/modules/README.statistics
%doc %{_docdir}/kamailio/modules/README.stun
%doc %{_docdir}/kamailio/modules/README.textops
%doc %{_docdir}/kamailio/modules/README.textopsx
%doc %{_docdir}/kamailio/modules/README.timer
%doc %{_docdir}/kamailio/modules/README.tm
%doc %{_docdir}/kamailio/modules/README.tmrec
%doc %{_docdir}/kamailio/modules/README.tmx
%doc %{_docdir}/kamailio/modules/README.topoh
%doc %{_docdir}/kamailio/modules/README.uac
%doc %{_docdir}/kamailio/modules/README.uac_redirect
%doc %{_docdir}/kamailio/modules/README.uid_auth_db
%doc %{_docdir}/kamailio/modules/README.uid_avp_db
%doc %{_docdir}/kamailio/modules/README.uid_domain
%doc %{_docdir}/kamailio/modules/README.uid_gflags
%doc %{_docdir}/kamailio/modules/README.uid_uri_db
%doc %{_docdir}/kamailio/modules/README.uri_db
%doc %{_docdir}/kamailio/modules/README.userblacklist
%doc %{_docdir}/kamailio/modules/README.usrloc
%doc %{_docdir}/kamailio/modules/README.xhttp
%doc %{_docdir}/kamailio/modules/README.xhttp_rpc
%doc %{_docdir}/kamailio/modules/README.xlog
%doc %{_docdir}/kamailio/modules/README.xprint
%doc %{_docdir}/kamailio/modules/README.jsonrpc-s
%doc %{_docdir}/kamailio/modules/README.nosip
%doc %{_docdir}/kamailio/modules/README.tsilo


%dir %attr(-,kamailio,kamailio) %{_sysconfdir}/kamailio
%config(noreplace) %{_sysconfdir}/kamailio/*
%config %{_sysconfdir}/rc.d/init.d/*
%config %{_sysconfdir}/sysconfig/*

%dir %{_libdir}/kamailio
%{_libdir}/kamailio/libbinrpc.so
%{_libdir}/kamailio/libbinrpc.so.0
%{_libdir}/kamailio/libbinrpc.so.0.1
%{_libdir}/kamailio/libkcore.so
%{_libdir}/kamailio/libkcore.so.1
%{_libdir}/kamailio/libkcore.so.1.0
%{_libdir}/kamailio/libkmi.so
%{_libdir}/kamailio/libkmi.so.1
%{_libdir}/kamailio/libkmi.so.1.0
%{_libdir}/kamailio/libprint.so
%{_libdir}/kamailio/libprint.so.1
%{_libdir}/kamailio/libprint.so.1.2
%{_libdir}/kamailio/libsrdb1.so
%{_libdir}/kamailio/libsrdb1.so.1
%{_libdir}/kamailio/libsrdb1.so.1.0
%{_libdir}/kamailio/libsrdb2.so
%{_libdir}/kamailio/libsrdb2.so.1
%{_libdir}/kamailio/libsrdb2.so.1.0
%{_libdir}/kamailio/libsrutils.so
%{_libdir}/kamailio/libsrutils.so.1
%{_libdir}/kamailio/libsrutils.so.1.0
%{_libdir}/kamailio/libtrie.so
%{_libdir}/kamailio/libtrie.so.1
%{_libdir}/kamailio/libtrie.so.1.0

%dir %{_libdir}/kamailio/modules
%{_libdir}/kamailio/modules/acc.so
%{_libdir}/kamailio/modules/alias_db.so
%{_libdir}/kamailio/modules/async.so
%{_libdir}/kamailio/modules/auth.so
%{_libdir}/kamailio/modules/auth_db.so
%{_libdir}/kamailio/modules/auth_diameter.so
%{_libdir}/kamailio/modules/avp.so
%{_libdir}/kamailio/modules/avpops.so
%{_libdir}/kamailio/modules/benchmark.so
%{_libdir}/kamailio/modules/blst.so
%{_libdir}/kamailio/modules/call_control.so
%{_libdir}/kamailio/modules/cfg_db.so
%{_libdir}/kamailio/modules/cfg_rpc.so
%{_libdir}/kamailio/modules/cfgutils.so
%{_libdir}/kamailio/modules/cnxcc.so
%{_libdir}/kamailio/modules/corex.so
%{_libdir}/kamailio/modules/counters.so
%{_libdir}/kamailio/modules/ctl.so
%{_libdir}/kamailio/modules/db_cluster.so
%{_libdir}/kamailio/modules/db_flatstore.so
%{_libdir}/kamailio/modules/db_text.so
%{_libdir}/kamailio/modules/db2_ops.so
%{_libdir}/kamailio/modules/debugger.so
%{_libdir}/kamailio/modules/dialog.so
%{_libdir}/kamailio/modules/dialog_ng.so
%{_libdir}/kamailio/modules/dispatcher.so
%{_libdir}/kamailio/modules/diversion.so
%{_libdir}/kamailio/modules/dmq.so
%{_libdir}/kamailio/modules/domain.so
%{_libdir}/kamailio/modules/domainpolicy.so
%{_libdir}/kamailio/modules/drouting.so
%{_libdir}/kamailio/modules/enum.so
%{_libdir}/kamailio/modules/exec.so
%{_libdir}/kamailio/modules/group.so
%{_libdir}/kamailio/modules/htable.so
%{_libdir}/kamailio/modules/imc.so
%{_libdir}/kamailio/modules/ipops.so
%{_libdir}/kamailio/modules/kex.so
%{_libdir}/kamailio/modules/malloc_test.so
%{_libdir}/kamailio/modules/mangler.so
%{_libdir}/kamailio/modules/matrix.so
%{_libdir}/kamailio/modules/maxfwd.so
%{_libdir}/kamailio/modules/mediaproxy.so
%{_libdir}/kamailio/modules/mi_datagram.so
%{_libdir}/kamailio/modules/mi_fifo.so
%{_libdir}/kamailio/modules/mi_rpc.so
%{_libdir}/kamailio/modules/mohqueue.so
%{_libdir}/kamailio/modules/mqueue.so
%{_libdir}/kamailio/modules/msilo.so
%{_libdir}/kamailio/modules/msrp.so
%{_libdir}/kamailio/modules/mtree.so
%{_libdir}/kamailio/modules/nat_traversal.so
%{_libdir}/kamailio/modules/nathelper.so
%{_libdir}/kamailio/modules/p_usrloc.so
%{_libdir}/kamailio/modules/path.so
%{_libdir}/kamailio/modules/pdb.so
%{_libdir}/kamailio/modules/pdt.so
%{_libdir}/kamailio/modules/permissions.so
%{_libdir}/kamailio/modules/pike.so
%{_libdir}/kamailio/modules/pipelimit.so
%{_libdir}/kamailio/modules/prefix_route.so
%{_libdir}/kamailio/modules/print.so
%{_libdir}/kamailio/modules/print_lib.so
%{_libdir}/kamailio/modules/pv.so
%{_libdir}/kamailio/modules/qos.so
%{_libdir}/kamailio/modules/ratelimit.so
%{_libdir}/kamailio/modules/registrar.so
%{_libdir}/kamailio/modules/rr.so
%{_libdir}/kamailio/modules/rtimer.so
%{_libdir}/kamailio/modules/rtpproxy.so
%{_libdir}/kamailio/modules/rtpengine.so
%{_libdir}/kamailio/modules/sanity.so
%{_libdir}/kamailio/modules/sca.so
%{_libdir}/kamailio/modules/sdpops.so
%{_libdir}/kamailio/modules/seas.so
%{_libdir}/kamailio/modules/sipcapture.so
%{_libdir}/kamailio/modules/sipt.so
%{_libdir}/kamailio/modules/siptrace.so
%{_libdir}/kamailio/modules/siputils.so
%{_libdir}/kamailio/modules/sl.so
%{_libdir}/kamailio/modules/sms.so
%{_libdir}/kamailio/modules/speeddial.so
%{_libdir}/kamailio/modules/sqlops.so
%{_libdir}/kamailio/modules/sst.so
%{_libdir}/kamailio/modules/statistics.so
%{_libdir}/kamailio/modules/stun.so
%{_libdir}/kamailio/modules/textops.so
%{_libdir}/kamailio/modules/textopsx.so
%{_libdir}/kamailio/modules/timer.so
%{_libdir}/kamailio/modules/tm.so
%{_libdir}/kamailio/modules/tmrec.so
%{_libdir}/kamailio/modules/tmx.so
%{_libdir}/kamailio/modules/topoh.so
%{_libdir}/kamailio/modules/uac.so
%{_libdir}/kamailio/modules/uac_redirect.so
%{_libdir}/kamailio/modules/uid_auth_db.so
%{_libdir}/kamailio/modules/uid_avp_db.so
%{_libdir}/kamailio/modules/uid_domain.so
%{_libdir}/kamailio/modules/uid_gflags.so
%{_libdir}/kamailio/modules/uid_uri_db.so
%{_libdir}/kamailio/modules/uri_db.so
%{_libdir}/kamailio/modules/userblacklist.so
%{_libdir}/kamailio/modules/usrloc.so
%{_libdir}/kamailio/modules/xhttp.so
%{_libdir}/kamailio/modules/xhttp_rpc.so
%{_libdir}/kamailio/modules/xlog.so
%{_libdir}/kamailio/modules/xprint.so
%{_libdir}/kamailio/modules/jsonrpc-s.so
%{_libdir}/kamailio/modules/nosip.so
%{_libdir}/kamailio/modules/tsilo.so


%{_sbindir}/kamailio
%{_sbindir}/kamctl
%{_sbindir}/kamdbctl
%{_sbindir}/kamcmd

%dir %{_libdir}/kamailio/kamctl
%{_libdir}/kamailio/kamctl/kamctl.base
%{_libdir}/kamailio/kamctl/kamctl.ctlbase
%{_libdir}/kamailio/kamctl/kamctl.dbtext
%{_libdir}/kamailio/kamctl/kamctl.fifo
%{_libdir}/kamailio/kamctl/kamctl.ser
%{_libdir}/kamailio/kamctl/kamctl.ser_mi
%{_libdir}/kamailio/kamctl/kamctl.sqlbase
%{_libdir}/kamailio/kamctl/kamctl.unixsock
%{_libdir}/kamailio/kamctl/kamdbctl.base
%{_libdir}/kamailio/kamctl/kamdbctl.dbtext

%dir %{_libdir}/kamailio/kamctl/dbtextdb
%{_libdir}/kamailio/kamctl/dbtextdb/dbtextdb.py
%{_libdir}/kamailio/kamctl/dbtextdb/dbtextdb.pyc
%{_libdir}/kamailio/kamctl/dbtextdb/dbtextdb.pyo

%{_mandir}/man5/*
%{_mandir}/man8/*

%dir %{_datadir}/kamailio
%dir %{_datadir}/kamailio/dbtext
%dir %{_datadir}/kamailio/dbtext/kamailio
%{_datadir}/kamailio/dbtext/kamailio/*


%files		auth-ephemeral
%defattr(-,root,root)
%doc %{_docdir}/kamailio/modules/README.auth_ephemeral
%{_libdir}/kamailio/modules/auth_ephemeral.so


%files		bdb
%defattr(-,root,root)
%doc %{_docdir}/kamailio/modules/README.db_berkeley
%{_sbindir}/kambdb_recover
%{_libdir}/kamailio/modules/db_berkeley.so
%{_libdir}/kamailio/kamctl/kamctl.db_berkeley
%{_libdir}/kamailio/kamctl/kamdbctl.db_berkeley
%dir %{_datadir}/kamailio/db_berkeley
%{_datadir}/kamailio/db_berkeley/*


%files		carrierroute
%defattr(-,root,root)
%{_docdir}/kamailio/modules/README.carrierroute
%{_libdir}/kamailio/modules/carrierroute.so


%files		cpl
%defattr(-,root,root)
%{_docdir}/kamailio/modules/README.cpl-c
%{_libdir}/kamailio/modules/cpl-c.so


%files		dialplan
%defattr(-,root,root)
%doc %{_docdir}/kamailio/modules/README.dialplan
%{_libdir}/kamailio/modules/dialplan.so


%files		dnssec
%defattr(-,root,root)
%doc %{_docdir}/kamailio/modules/README.dnssec
%{_libdir}/kamailio/modules/dnssec.so


%files		geoip
%defattr(-,root,root)
%doc %{_docdir}/kamailio/modules/README.geoip
%{_libdir}/kamailio/modules/geoip.so


%files		gzcompress
%defattr(-,root,root)
%doc %{_docdir}/kamailio/modules/README.gzcompress
%{_libdir}/kamailio/modules/gzcompress.so


%files		ims
%defattr(-,root,root)
%{_libdir}/kamailio/libkamailio_ims.so
%{_libdir}/kamailio/libkamailio_ims.so.0
%{_libdir}/kamailio/libkamailio_ims.so.0.1

%doc %{_docdir}/kamailio/modules/README.cdp
%doc %{_docdir}/kamailio/modules/README.cdp_avp
%doc %{_docdir}/kamailio/modules/README.ims_auth
%doc %{_docdir}/kamailio/modules/README.ims_charging
%doc %{_docdir}/kamailio/modules/README.ims_icscf
%doc %{_docdir}/kamailio/modules/README.ims_isc
%doc %{_docdir}/kamailio/modules/README.ims_qos
#%doc %{_docdir}/kamailio/modules/README.ims_registrar_pcscf
#%doc %{_docdir}/kamailio/modules/README.ims_registrar_scscf
%doc %{_docdir}/kamailio/modules/README.ims_usrloc_pcscf
#%doc %{_docdir}/kamailio/modules/README.ims_usrloc_scscf
%{_libdir}/kamailio/modules/cdp.so
%{_libdir}/kamailio/modules/cdp_avp.so
%{_libdir}/kamailio/modules/ims_auth.so
%{_libdir}/kamailio/modules/ims_charging.so
%{_libdir}/kamailio/modules/ims_icscf.so
%{_libdir}/kamailio/modules/ims_isc.so
%{_libdir}/kamailio/modules/ims_qos.so
%{_libdir}/kamailio/modules/ims_registrar_pcscf.so
%{_libdir}/kamailio/modules/ims_registrar_scscf.so
%{_libdir}/kamailio/modules/ims_usrloc_pcscf.so
%{_libdir}/kamailio/modules/ims_usrloc_scscf.so


%files		java
%defattr(-,root,root)
%doc %{_docdir}/kamailio/modules/README.app_java
%{_libdir}/kamailio/modules/app_java.so
%dir %{_libdir}/kamailio/java
%{_libdir}/kamailio/java/Kamailio.class
%{_libdir}/kamailio/java/kamailio.jar


%files		json
%defattr(-,root,root)
%doc %{_docdir}/kamailio/modules/README.json
%doc %{_docdir}/kamailio/modules/README.jsonrpc-c
%{_libdir}/kamailio/modules/json.so
%{_libdir}/kamailio/modules/jsonrpc-c.so


%files		lcr
%defattr(-,root,root)
%doc %{_docdir}/kamailio/modules/README.lcr
%{_libdir}/kamailio/modules/lcr.so


%files		ldap
%defattr(-,root,root)
%doc %{_docdir}/kamailio/modules/README.db2_ldap
%doc %{_docdir}/kamailio/modules/README.h350
%doc %{_docdir}/kamailio/modules/README.ldap
%{_libdir}/kamailio/modules/db2_ldap.so
%{_libdir}/kamailio/modules/h350.so
%{_libdir}/kamailio/modules/ldap.so


%files		lua
%defattr(-,root,root)
%doc %{_docdir}/kamailio/modules/README.app_lua
%{_libdir}/kamailio/modules/app_lua.so


%files		memcached
%defattr(-,root,root)
%doc %{_docdir}/kamailio/modules/README.memcached
%{_libdir}/kamailio/modules/memcached.so


%files		mysql
%defattr(-,root,root)
%doc %{_docdir}/kamailio/modules/README.db_mysql
%{_libdir}/kamailio/modules/db_mysql.so
%{_libdir}/kamailio/kamctl/kamctl.mysql
%{_libdir}/kamailio/kamctl/kamdbctl.mysql
%dir %{_datadir}/kamailio/mysql
%{_datadir}/kamailio/mysql/*


%files		outbound
%defattr(-,root,root)
%doc %{_docdir}/kamailio/modules/README.outbound
%{_libdir}/kamailio/modules/outbound.so


%files		perl
%defattr(-,root,root)
%doc %{_docdir}/kamailio/modules/README.app_perl
%doc %{_docdir}/kamailio/modules/README.db_perlvdb
%{_libdir}/kamailio/modules/app_perl.so
%{_libdir}/kamailio/modules/db_perlvdb.so
%dir %{_libdir}/kamailio/perl
%{_libdir}/kamailio/perl/Kamailio.pm
%dir %{_libdir}/kamailio/perl/Kamailio
%{_libdir}/kamailio/perl/Kamailio/Constants.pm
%{_libdir}/kamailio/perl/Kamailio/Message.pm
%{_libdir}/kamailio/perl/Kamailio/VDB.pm
%dir %{_libdir}/kamailio/perl/Kamailio/LDAPUtils
%{_libdir}/kamailio/perl/Kamailio/LDAPUtils/LDAPConf.pm
%{_libdir}/kamailio/perl/Kamailio/LDAPUtils/LDAPConnection.pm
%dir %{_libdir}/kamailio/perl/Kamailio/Utils
%{_libdir}/kamailio/perl/Kamailio/Utils/Debug.pm
%{_libdir}/kamailio/perl/Kamailio/Utils/PhoneNumbers.pm
%dir %{_libdir}/kamailio/perl/Kamailio/VDB
%{_libdir}/kamailio/perl/Kamailio/VDB/Column.pm
%{_libdir}/kamailio/perl/Kamailio/VDB/Pair.pm
%{_libdir}/kamailio/perl/Kamailio/VDB/ReqCond.pm
%{_libdir}/kamailio/perl/Kamailio/VDB/Result.pm
%{_libdir}/kamailio/perl/Kamailio/VDB/VTab.pm
%{_libdir}/kamailio/perl/Kamailio/VDB/Value.pm
%dir %{_libdir}/kamailio/perl/Kamailio/VDB/Adapter
%{_libdir}/kamailio/perl/Kamailio/VDB/Adapter/AccountingSIPtrace.pm
%{_libdir}/kamailio/perl/Kamailio/VDB/Adapter/Alias.pm
%{_libdir}/kamailio/perl/Kamailio/VDB/Adapter/Auth.pm
%{_libdir}/kamailio/perl/Kamailio/VDB/Adapter/Describe.pm
%{_libdir}/kamailio/perl/Kamailio/VDB/Adapter/Speeddial.pm
%{_libdir}/kamailio/perl/Kamailio/VDB/Adapter/TableVersions.pm


%files		postgresql
%defattr(-,root,root)
%doc %{_docdir}/kamailio/modules/README.db_postgres
%{_libdir}/kamailio/modules/db_postgres.so
%{_libdir}/kamailio/kamctl/kamctl.pgsql
%{_libdir}/kamailio/kamctl/kamdbctl.pgsql
%dir %{_datadir}/kamailio/postgres
%{_datadir}/kamailio/postgres/*


%files		presence
%defattr(-,root,root)
%doc %{_docdir}/kamailio/modules/README.presence
%doc %{_docdir}/kamailio/modules/README.presence_conference
%doc %{_docdir}/kamailio/modules/README.presence_dialoginfo
%doc %{_docdir}/kamailio/modules/README.presence_mwi
%doc %{_docdir}/kamailio/modules/README.presence_profile
%doc %{_docdir}/kamailio/modules/README.presence_reginfo
%doc %{_docdir}/kamailio/modules/README.presence_xml
%doc %{_docdir}/kamailio/modules/README.pua
%doc %{_docdir}/kamailio/modules/README.pua_bla
%doc %{_docdir}/kamailio/modules/README.pua_dialoginfo
%doc %{_docdir}/kamailio/modules/README.pua_mi
%doc %{_docdir}/kamailio/modules/README.pua_reginfo
%doc %{_docdir}/kamailio/modules/README.pua_usrloc
%doc %{_docdir}/kamailio/modules/README.pua_xmpp
%doc %{_docdir}/kamailio/modules/README.rls
%doc %{_docdir}/kamailio/modules/README.xcap_client
%doc %{_docdir}/kamailio/modules/README.xcap_server
%{_libdir}/kamailio/modules/presence.so
%{_libdir}/kamailio/modules/presence_conference.so
%{_libdir}/kamailio/modules/presence_dialoginfo.so
%{_libdir}/kamailio/modules/presence_mwi.so
%{_libdir}/kamailio/modules/presence_profile.so
%{_libdir}/kamailio/modules/presence_reginfo.so
%{_libdir}/kamailio/modules/presence_xml.so
%{_libdir}/kamailio/modules/pua.so
%{_libdir}/kamailio/modules/pua_bla.so
%{_libdir}/kamailio/modules/pua_dialoginfo.so
%{_libdir}/kamailio/modules/pua_mi.so
%{_libdir}/kamailio/modules/pua_reginfo.so
%{_libdir}/kamailio/modules/pua_usrloc.so
%{_libdir}/kamailio/modules/pua_xmpp.so
%{_libdir}/kamailio/modules/rls.so
%{_libdir}/kamailio/modules/xcap_client.so
%{_libdir}/kamailio/modules/xcap_server.so


%files		purple
%defattr(-,root,root)
%doc %{_docdir}/kamailio/modules/README.purple
%{_libdir}/kamailio/modules/purple.so


%files		python
%defattr(-,root,root)
%doc %{_docdir}/kamailio/modules/README.app_python
%{_libdir}/kamailio/modules/app_python.so


%files		radius
%defattr(-,root,root)
%doc %{_docdir}/kamailio/modules/README.acc_radius
%doc %{_docdir}/kamailio/modules/README.auth_radius
%doc %{_docdir}/kamailio/modules/README.misc_radius
%doc %{_docdir}/kamailio/modules/README.peering
%{_libdir}/kamailio/modules/acc_radius.so
%{_libdir}/kamailio/modules/auth_radius.so
%{_libdir}/kamailio/modules/misc_radius.so
%{_libdir}/kamailio/modules/peering.so


%files		redis
%defattr(-,root,root)
%doc %{_docdir}/kamailio/modules/README.ndb_redis
%{_libdir}/kamailio/modules/ndb_redis.so


%files		regex
%defattr(-,root,root)
%doc %{_docdir}/kamailio/modules/README.regex
%{_libdir}/kamailio/modules/regex.so


%files		sctp
%defattr(-,root,root)
%doc %{_docdir}/kamailio/modules/README.sctp
%{_libdir}/kamailio/modules/sctp.so


%files		snmpstats
%defattr(-,root,root)
%{_docdir}/kamailio/modules/README.snmpstats
%{_libdir}/kamailio/modules/snmpstats.so
%{_datadir}/snmp/mibs/KAMAILIO-MIB
%{_datadir}/snmp/mibs/KAMAILIO-REG-MIB
%{_datadir}/snmp/mibs/KAMAILIO-SIP-COMMON-MIB
%{_datadir}/snmp/mibs/KAMAILIO-SIP-SERVER-MIB
%{_datadir}/snmp/mibs/KAMAILIO-TC


%files		sqlite
%defattr(-,root,root)
%doc %{_docdir}/kamailio/modules/README.db_sqlite
%{_libdir}/kamailio/modules/db_sqlite.so
%{_libdir}/kamailio/kamctl/kamctl.sqlite
%{_libdir}/kamailio/kamctl/kamdbctl.sqlite
%dir %{_datadir}/kamailio/db_sqlite
%{_datadir}/kamailio/db_sqlite/*


%files		tls
%defattr(-,root,root)
%doc %{_docdir}/kamailio/modules/README.auth_identity
%doc %{_docdir}/kamailio/modules/README.tls
%{_libdir}/kamailio/modules/auth_identity.so
%{_libdir}/kamailio/modules/tls.so


%files		unixodbc
%defattr(-,root,root)
%doc %{_docdir}/kamailio/modules/README.db_unixodbc
%{_libdir}/kamailio/modules/db_unixodbc.so


%files		utils
%defattr(-,root,root)
%{_docdir}/kamailio/modules/README.utils
%{_libdir}/kamailio/modules/utils.so


%files		websocket
%defattr(-,root,root)
%doc %{_docdir}/kamailio/modules/README.websocket
%{_libdir}/kamailio/modules/websocket.so


%files		xhttp-pi
%defattr(-,root,root)
%doc %{_docdir}/kamailio/modules/README.xhttp_pi
%{_libdir}/kamailio/modules/xhttp_pi.so
%dir %{_datadir}/kamailio/xhttp_pi
%{_datadir}/kamailio/xhttp_pi/*


%files		xmlops
%defattr(-,root,root)
%doc %{_docdir}/kamailio/modules/README.xmlops
%{_libdir}/kamailio/modules/xmlops.so


%files		xmlrpc
%defattr(-,root,root)
%doc %{_docdir}/kamailio/modules/README.xmlrpc
%{_libdir}/kamailio/modules/xmlrpc.so
%doc %{_docdir}/kamailio/modules/README.mi_xmlrpc
%{_libdir}/kamailio/modules/mi_xmlrpc.so


%files		xmpp
%defattr(-,root,root)
%doc %{_docdir}/kamailio/modules/README.xmpp
%{_libdir}/kamailio/modules/xmpp.so



%changelog
* Tue Dec 3 2013 Peter Dunkley <peter.dunkley@crocodilertc.net>
  - Updated version to 4.2.0
* Mon Oct 7 2013 Peter Dunkley <peter.dunkley@crocodilertc.net>
  - Consolidating changelog for 4.1.0 into a single entry...
  - Added new modules to main package:
    - cnxcc
    - gzcompress
    - mohqueue
    - rtpproxy-ng
    - sipt
    - stun (STUN functionality moved from compile time in core to own module)
  - Added new modules to other packages:
    - ims_charging module to ims package
  - Added new packages for new modules:
    - app_java
    - auth_ephemeral
    - sctp (SCTP functionality moved from compile time in core to own module)
  - Moved existing modules to different packages:
    - auth_identity to tls package (previously not built for CentOS)
    - cdp and cdp_avp to ims package
    - dialog_ng to main package
    - memcached to own package (previously not built for CentOS)
    - mi_xmlrpc to own package (previously not built for CentOS)
    - tls to own package
  - Added packages for (new and existing) modules that require EPEL:
    - carrierroute in own package
    - dnssec in own package
    - geoip in own package
    - json and jsonrpc-c in new json package
    - redis in own package
    - acc_radius, auth_radius, misc_radius, and peering in new radius package
  - Removed Fedora stuff as I am only maintaining this for CentOS now
  - Refactored .spec
  - Updated make commands to match updated module groups
  - Updated version to 4.1.0
* Mon Mar 11 2013 Peter Dunkley <peter.dunkley@crocodilertc.net>
  - Consolidating changelog for 4.0.0 into a single entry...
  - Added new modules to main package:
    - corex
    - sca
  - Added new packages for new modules:
    - cdp (cdp, cdp_avp)
    - ims (dialog_ng, ims_auth, ims_icscf, ims_isc, ims_qos,
      ims_registrar_pcscf, ims_registrar_scscf, ims_usrloc_pcscf,
      ims_usrloc_scscf)
    - outbound
    - websocket
    - xhttp_pi
  - Moved existing modules to different packages:
    - Various SER modules added to main package (avp, db2_ops, mangler, timer,
      uid_auth_db, uid_avp_db, uid_domain, uid_gflags, uid_uri_db, print,
      print_lib, xprint)
    - db2_ldap SER module added to ldap package
    - tls to main package (as OpenSSL was needed in core for STUN)
  - Moved modules from modules_k/ to modules/
  - Renamed perl modules
  - Added installation of auth.7.gz for Fedora now that manpages are built for
    Fedora
  - SCTP and STUN now included in this build
  - Refactored .spec
  - Updated ver to 4.0.0
* Mon Jun 18 2012 Peter Dunkley <peter.dunkley@crocodilertc.net>
  - Consolidating changelog for 3.3.0 into a single entry...
  - See revision control for details this far back
