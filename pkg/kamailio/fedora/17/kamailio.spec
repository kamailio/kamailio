%define name	kamailio
%define ver	4.3.2
%define rel	0%{dist}



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
Conflicts:	kamailio-auth-ephemeral < %ver, kamailio-auth-identity < %ver
Conflicts:	kamailio-bdb < %ver, kamailio-cdp < %ver, kamailio-cdp < %ver
Conflicts:	kamailio-dialplan < %ver, kamailio-ims < %ver
Conflicts:	kamailio-lcr < %ver, kamailio-ldap < %ver, kamailio-lua < %ver
Conflicts:	kamailio-mysql < %ver, kamailio-outbound < %ver
Conflicts:	kamailio-perl < %ver, kamailio-postgresql < %ver
Conflicts:	kamailio-presence < %ver, kamailio-purple < %ver
Conflicts:	kamailio-python < %ver, kamailio-regex < %ver
Conflicts:	kamailio-sctp < %ver, kamailio-snmpstats < %ver
Conflicts:	kamailio-sqlite < %ver, kamailio-stun < %ver
Conflicts:	kamailio-tls < %ver, kamailio-unixODBC < %ver
Conflicts:	kamailio-utils < %ver, kamailio-websocket < %ver
Conflicts:	kamailio-xhttp-pi < %ver, kamailio-xmlops < %ver
Conflicts:	kamailio-xmlrpc < %ver, kamailio-xmpp < %ver
%if 0%{?fedora}
Conflicts:	kamailio-carrierroute < %ver, kamailio-GeoIP < %ver
Conflicts:	kamailio-json < %ver, kamailio-mono < %ver
Conflicts: 	kamailio-radius < %ver, kamailio-redis < %ver
%endif
BuildRequires:	bison, flex, gcc, make, redhat-rpm-config
%if 0%{?fedora}
BuildRequires:	docbook2X
%endif

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


%package	auth-identity
Summary:	Functions for secure identification of originators of SIP messages for Kamailio.
Group:		System Environment/Daemons
Requires:	libcurl, openssl, kamailio = %ver
BuildRequires:	libcurl-devel, openssl-devel

%description auth-identity
Functions for secure identification of originators of SIP messages for Kamailio.


%package	bdb
Summary:	Berkeley database connectivity for Kamailio.
Group:		System Environment/Daemons
Requires:	db4, kamailio = %ver
BuildRequires:	db4-devel

%description	bdb
Berkeley database connectivity for Kamailio.


%package	cdp
Summary:	C Diameter Peer module and extensions module for Kamailio.
Group:		System Environment/Daemons
Requires:	libxml2, kamailio = %ver
BuildRequires:	libxml2-devel

%description	cdp
C Diameter Peer module and extensions module for Kamailio.


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


%package	ims
Summary:	IMS modules and extensions module for Kamailio.
Group:		System Environment/Daemons
Requires:	libxml2, kamailio = %ver, kamailio-cdp = %ver
BuildRequires:	libxml2-devel

%description	ims
IMS modules and extensions module for Kamailio.


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
%if 0%{?fedora}
Requires:	glib, libpurple, libxml2, kamailio = %ver
Requires:	kamailio-presence = %ver
BuildRequires:	glib-devel, libpurple-devel, libxml2-devel
%else
Requires:	glib2, libpurple, libxml2, kamailio = %ver
Requires:	kamailio-presence = %ver
BuildRequires:	glib2-devel, libpurple-devel, libxml2-devel
%endif

%description	purple
Multi-protocol IM and presence gateway module.


%package	python
Summary:	Python extensions for Kamailio.
Group:		System Environment/Daemons
Requires:	python, kamailio = %ver
BuildRequires:	python-devel

%description	python
Python extensions for Kamailio.


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
%if 0%{?fedora}
Requires:	net-snmp-agent-libs, kamailio = %ver
%else
Requires:	net-snmp-libs, kamailio = %ver
%endif
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


%package	stun
Summary:	Limited STUN (RFC 5389) support for Kamailio.
Group:		System Environment/Daemons
Requires:	openssl, kamailio = %ver
BuildRequires:	openssl-devel

%description	stun
Limited RFC 5389, "Session Traversal Utilities for NAT (STUN)" support for
Kamailio.


%package	tls
Summary:	TLS transport for Kamailio.
Group:		System Environment/Daemons
Requires:	openssl, kamailio = %ver
BuildRequires:	openssl-devel

%description	tls
TLS transport for Kamailio.


%package	unixODBC
Summary:	unixODBC database connectivity for Kamailio.
Group:		System Environment/Daemons
Requires:	unixODBC, kamailio = %ver
BuildRequires:	unixODBC-devel

%description	unixODBC
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
Summary:	XMLRPC trasnport and encoding for Kamailio RPCs.
Group:		System Environment/Daemons
Requires:	libxml2, kamailio = %ver
BuildRequires:	libxml2-devel

%description	xmlrpc
XMLRPC trasnport and encoding for Kamailio RPCs.


%package	xmpp
Summary:	SIP/XMPP IM gateway for Kamailio.
Group:		System Environment/Daemons
Requires:	expat, kamailio = %ver
BuildRequires:	expat-devel

%description	xmpp
SIP/XMPP IM gateway for Kamailio.


%if 0%{?fedora}
%package	carrierroute
Summary:	Routing, balancing, and blacklisting for Kamailio.
Group:		System Environment/Daemons
Requires:	libconfuse, kamailio = %ver
BuildRequires:	libconfuse-devel

%description	carrierroute
Routing, balancing, and blacklisting for Kamailio.


%package	GeoIP
Summary:	Max Mind GeoIP real-time query support for Kamailio.
Group:		System Environment/Daemons
Requires:	GeoIP, kamailio = %ver
BuildRequires:	GeoIP-devel

%description	GeoIP
Max Mind GeoIP real-time query support for Kamailio.


%package	json
Summary:	json string operation and rpc support for Kamailio.
Group:		System Environment/Daemons
Requires:	json-c, libevent, kamailio = %ver
BuildRequires:	json-c-devel, libevent-devel

%description	json
json string operation and rpc support for Kamailio.


%package	mono
Summary:	Mono extensions for Kamailio.
Group:		System Environment/Daemons
Requires:	mono-core, kamailio = %ver
BuildRequires:	mono-devel

%description	mono
Mono extensions for Kamailio.


%package	radius
Summary:	Radius AAA API for Kamailio.
Group:		System Environment/Daemons
Requires:	radiusclient-ng, kamailio = %ver
BuildRequires:	radiusclient-ng-devel

%description	radius
Radius AAA API for Kamailio.


%package	redis
Summary:	REDIS NoSQL database connector for Kamailio.
Group:		System Environment/Daemons
Requires:	hiredis, kamailio = %ver
BuildRequires:	hiredis-devel

%description	redis
REDIS NoSQL database connector for Kamailio.
%endif



%prep
%setup -n %{name}-%{ver}



%build
make cfg prefix=/usr cfg_prefix=$RPM_BUILD_ROOT basedir=$RPM_BUILD_ROOT \
	cfg_target=/%{_sysconfdir}/kamailio/ modules_dirs="modules"
make
%if 0%{?fedora}
make every-module skip_modules="app_java db_cassandra db_oracle dnssec \
	iptrtpproxy memcached mi_xmlrpc osp" \
	group_include="kstandard kmysql kpostgres kcpl kxml kradius kunixodbc \
	kperl ksnmpstats kxmpp kcarrierroute kberkeley kldap kutils kpurple \
	ktls kwebsocket kpresence klua kpython kgeoip ksqlite kjson kredis \
	kmono kims koutbound ksctp kstun kautheph"
%else
make every-module skip_modules="app_java db_cassandra db_oracle dnssec \
	iptrtpproxy memcached mi_xmlrpc osp" \
	group_include="kstandard kmysql kpostgres kcpl kxml kunixodbc \
	kperl ksnmpstats kxmpp kberkeley kldap kutils kpurple \
	ktls kwebsocket kpresence klua kpython ksqlite \
	kims koutbound ksctp kstun kautheph"
%endif
make utils



%install
[ "$RPM_BUILD_ROOT" != "/" ] && rm -rf "$RPM_BUILD_ROOT"

make install
%if 0%{?fedora}
make install-modules-all skip_modules="db_cassandra iptrtpproxy db_oracle \
	memcached mi_xmlrpc osp" \
	group_include="kstandard kmysql kpostgres kcpl kxml kradius kunixodbc \
	kperl ksnmpstats kxmpp kcarrierroute kberkeley kldap kutils kpurple \
	ktls kwebsocket kpresence klua kpython kgeoip ksqlite kjson kredis \
	kmono kims koutbound ksctp kstun kautheph"

mkdir -p $RPM_BUILD_ROOT/%{_unitdir}
install -m644 pkg/kamailio/fedora/%{?fedora}/kamailio.service \
		$RPM_BUILD_ROOT/%{_unitdir}/kamailio.service

mkdir -p $RPM_BUILD_ROOT/%{_sysconfdir}/sysconfig
install -m644 pkg/kamailio/fedora/%{?fedora}/kamailio.sysconfig \
		$RPM_BUILD_ROOT/%{_sysconfdir}/sysconfig/kamailio
%else
make install-modules-all skip_modules="db_cassandra iptrtpproxy db_oracle \
	memcached mi_xmlrpc osp" \
	group_include="kstandard kmysql kpostgres kcpl kxml kunixodbc \
	kperl ksnmpstats kxmpp kberkeley kldap kutils kpurple \
	ktls kwebsocket kpresence klua kpython ksqlite \
	kims koutbound ksctp kstun kautheph"

mkdir -p $RPM_BUILD_ROOT/%{_sysconfdir}/rc.d/init.d
install -m755 pkg/kamailio/centos/%{?centos}/kamailio.init \
		$RPM_BUILD_ROOT/%{_sysconfdir}/rc.d/init.d/kamailio

mkdir -p $RPM_BUILD_ROOT/%{_sysconfdir}/sysconfig
install -m644 pkg/kamailio/centos/%{?centos}/kamailio.sysconfig \
		$RPM_BUILD_ROOT/%{_sysconfdir}/sysconfig/kamailio
%endif



%pre
/usr/sbin/groupadd -r kamailio 2> /dev/null || :
/usr/sbin/useradd -r -g kamailio -s /bin/false -c "Kamailio daemon" -d \
		%{_libdir}/kamailio kamailio 2> /dev/null || :



%clean
rm -rf "$RPM_BUILD_ROOT"



%post
%if 0%{?fedora}
/bin/systemctl --system daemon-reload
%else
/sbin/chkconfig --add kamailio
%endif



%preun
if [ $1 = 0 ]; then
%if 0%{?fedora}
	/bin/systemctl stop kamailio.service
	/bin/systemctl disable kamailio.service 2> /dev/null
%else
	/sbin/service kamailio stop > /dev/null 2>&1
	/sbin/chkconfig --del kamailio
%endif
fi



%postun
%if 0%{?fedora}
/bin/systemctl --system daemon-reload
%endif



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
%doc %{_docdir}/kamailio/modules/README.rtpproxy-ng
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

%dir %attr(-,kamailio,kamailio) %{_sysconfdir}/kamailio
%config(noreplace) %{_sysconfdir}/kamailio/*
%if 0%{?fedora}
%config %{_unitdir}/*
%else
%config %{_sysconfdir}/rc.d/init.d/*
%endif
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
%{_libdir}/kamailio/modules/auth.so
%{_libdir}/kamailio/modules/auth_db.so
%{_libdir}/kamailio/modules/auth_diameter.so
%{_libdir}/kamailio/modules/async.so
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
%{_libdir}/kamailio/modules/db_text.so
%{_libdir}/kamailio/modules/db_flatstore.so
%{_libdir}/kamailio/modules/db2_ops.so
%{_libdir}/kamailio/modules/debugger.so
%{_libdir}/kamailio/modules/dialog.so
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
%{_libdir}/kamailio/modules/rtpproxy-ng.so
%{_libdir}/kamailio/modules/sanity.so
%{_libdir}/kamailio/modules/sca.so
%{_libdir}/kamailio/modules/seas.so
%{_libdir}/kamailio/modules/sipcapture.so
%{_libdir}/kamailio/modules/sipt.so
%{_libdir}/kamailio/modules/siptrace.so
%{_libdir}/kamailio/modules/siputils.so
%{_libdir}/kamailio/modules/sl.so
%{_libdir}/kamailio/modules/sdpops.so
%{_libdir}/kamailio/modules/sms.so
%{_libdir}/kamailio/modules/speeddial.so
%{_libdir}/kamailio/modules/sqlops.so
%{_libdir}/kamailio/modules/sst.so
%{_libdir}/kamailio/modules/statistics.so
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
%if 0%{?fedora}
%{_mandir}/man7/*
%endif
%{_mandir}/man8/*

%dir %{_datadir}/kamailio
%dir %{_datadir}/kamailio/dbtext
%dir %{_datadir}/kamailio/dbtext/kamailio
%{_datadir}/kamailio/dbtext/kamailio/*


%files		auth-ephemeral
%defattr(-,root,root)
%doc %{_docdir}/kamailio/modules/README.auth_ephemeral
%{_libdir}/kamailio/modules/auth_ephemeral.so


%files		auth-identity
%defattr(-,root,root)
%doc %{_docdir}/kamailio/modules/README.auth_identity
%{_libdir}/kamailio/modules/auth_identity.so


%files		bdb
%defattr(-,root,root)
%doc %{_docdir}/kamailio/modules/README.db_berkeley
%{_sbindir}/kambdb_recover
%{_libdir}/kamailio/modules/db_berkeley.so
%{_libdir}/kamailio/kamctl/kamctl.db_berkeley
%{_libdir}/kamailio/kamctl/kamdbctl.db_berkeley
%dir %{_datadir}/kamailio/db_berkeley
%{_datadir}/kamailio/db_berkeley/*


%files		cdp
%defattr(-,root,root)
%doc %{_docdir}/kamailio/modules/README.cdp
%{_libdir}/kamailio/modules/cdp.so
%doc %{_docdir}/kamailio/modules/README.cdp_avp
%{_libdir}/kamailio/modules/cdp_avp.so


%files		cpl
%defattr(-,root,root)
%{_docdir}/kamailio/modules/README.cpl-c
%{_libdir}/kamailio/modules/cpl-c.so


%files		dialplan
%defattr(-,root,root)
%doc %{_docdir}/kamailio/modules/README.dialplan
%{_libdir}/kamailio/modules/dialplan.so


%files		ims
%defattr(-,root,root)
%{_libdir}/kamailio/libkamailio_ims.so
%{_libdir}/kamailio/libkamailio_ims.so.0
%{_libdir}/kamailio/libkamailio_ims.so.0.1
%doc %{_docdir}/kamailio/modules/README.dialog_ng
%{_libdir}/kamailio/modules/dialog_ng.so
%doc %{_docdir}/kamailio/modules/README.ims_auth
%{_libdir}/kamailio/modules/ims_auth.so
%doc %{_docdir}/kamailio/modules/README.ims_icscf
%{_libdir}/kamailio/modules/ims_icscf.so
%doc %{_docdir}/kamailio/modules/README.ims_isc
%{_libdir}/kamailio/modules/ims_isc.so
%doc %{_docdir}/kamailio/modules/README.ims_qos
%{_libdir}/kamailio/modules/ims_qos.so
#%doc %{_docdir}/kamailio/modules/README.ims_registrar_pcscf
%{_libdir}/kamailio/modules/ims_registrar_pcscf.so
#%doc %{_docdir}/kamailio/modules/README.ims_registrar_scscf
%{_libdir}/kamailio/modules/ims_registrar_scscf.so
%doc %{_docdir}/kamailio/modules/README.ims_usrloc_pcscf
%{_libdir}/kamailio/modules/ims_usrloc_pcscf.so
#%doc %{_docdir}/kamailio/modules/README.ims_usrloc_scscf
%{_libdir}/kamailio/modules/ims_usrloc_scscf.so


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


%files		sqlite
%defattr(-,root,root)
%doc %{_docdir}/kamailio/modules/README.db_sqlite
%{_libdir}/kamailio/modules/db_sqlite.so
%{_libdir}/kamailio/kamctl/kamctl.sqlite
%{_libdir}/kamailio/kamctl/kamdbctl.sqlite
%dir %{_datadir}/kamailio/db_sqlite
%{_datadir}/kamailio/db_sqlite/*


%files		stun
%defattr(-,root,root)
%doc %{_docdir}/kamailio/modules/README.stun
%{_libdir}/kamailio/modules/stun.so


%files		tls
%defattr(-,root,root)
%doc %{_docdir}/kamailio/modules/README.tls
%{_libdir}/kamailio/modules/tls.so


%files		unixODBC
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


%files		xmpp
%defattr(-,root,root)
%doc %{_docdir}/kamailio/modules/README.xmpp
%{_libdir}/kamailio/modules/xmpp.so


%if 0%{?fedora}
%files		carrierroute
%defattr(-,root,root)
%doc %{_docdir}/kamailio/modules/README.carrierroute
%{_libdir}/kamailio/modules/carrierroute.so


%files		radius
%defattr(-,root,root)
%{_docdir}/kamailio/modules/README.acc_radius
%{_docdir}/kamailio/modules/README.auth_radius
%{_docdir}/kamailio/modules/README.misc_radius
%{_docdir}/kamailio/modules/README.peering
%{_libdir}/kamailio/modules/acc_radius.so
%{_libdir}/kamailio/modules/auth_radius.so
%{_libdir}/kamailio/modules/misc_radius.so
%{_libdir}/kamailio/modules/peering.so


%files		json
%defattr(-,root,root)
%doc %{_docdir}/kamailio/modules/README.json
%doc %{_docdir}/kamailio/modules/README.jsonrpc-c
%{_libdir}/kamailio/modules/json.so
%{_libdir}/kamailio/modules/jsonrpc-c.so


%files		GeoIP
%defattr(-,root,root)
%doc %{_docdir}/kamailio/modules/README.geoip
%{_libdir}/kamailio/modules/geoip.so


%files		mono
%defattr(-,root,root)
%doc %{_docdir}/kamailio/modules/README.app_mono
%{_libdir}/kamailio/modules/app_mono.so


%files		redis
%defattr(-,root,root)
%doc %{_docdir}/kamailio/modules/README.ndb_redis
%{_libdir}/kamailio/modules/ndb_redis.so
%endif



%changelog
* Thu Aug 22 2013 Peter Dunkley <peter@dunkley.me.uk>
  - Added rtpproxy-ng module to build
* Wed Aug 14 2013 Peter Dunkley <peter@dunkley.me.uk>
  - Updated rel to dev7
* Mon May 27 2013 Peter Dunkley <peter@dunkley.me.uk>
  - Created package for auth_ephemeral module
* Sun May 26 2013 Peter Dunkley <peter@dunkley.me.uk>
  - Created package for sctp module
  - Updated rel to dev6
* Sat May 18 2013 Peter Dunkley <peter@dunkley.me.uk>
  - Refactored .spec
  - Put tls module back in its own .spec (OpenSSL no longer needed by core as
    stun is in its own module)
  - Updated rel to dev5
* Wed Apr 24 2013 Peter Dunkley <peter@dunkley.me.uk>
  - Updated rel to dev3
* Wed Apr 10 2013 Peter Dunkley <peter@dunkley.me.uk>
  - Added sipt module to .spec
  - Updated rel to dev2
* Fri Mar 29 2013 Peter Dunkley <peter@dunkley.me.uk>
  - Added stun module to .spec
  - Updated rel to dev1
* Wed Mar 27 2013 Peter Dunkley <peter@dunkley.me.uk>
  - Added cnxcc module to .spec
* Thu Mar 7 2013 Peter Dunkley <peter@dunkley.me.uk>
  - Added build requirement for docbook2X for Fedora builds
* Wed Mar 6 2013 Peter Dunkley <peter@dunkley.me.uk>
  - Restored perl related files
* Tue Mar 5 2013 Peter Dunkley <peter@dunkley.me.uk>
  - Updated rel to dev0 and ver to 4.1.0
  - Re-ordered file to make it internally consistent
  - Updated make commands to match updated module groups
  - Added auth_identity back in
  - Temporarily commented out perl related files as perl modules do not appear
    to be working
* Sun Jan 20 2013 Peter Dunkley <peter@dunkley.me.uk>
  - Updated rel to pre1
  - Moved modules from modules_k/ to modules/
  - Renamed perl modules
* Fri Jan 11 2013 Peter Dunkley <peter@dunkley.me.uk>
  - Updated rel to pre0
* Thu Jan 10 2013 Peter Dunkley <peter@dunkley.me.uk>
  - More IMS updates
* Tue Jan 8 2013 Peter Dunkley <peter@dunkley.me.uk>
  - Changed dialog2 to dialog_ng
  - Renamed all IMS modules (prepended ims_)
* Sun Jan 6 2013 Peter Dunkley <peter@dunkley.me.uk>
  - Updated ver to 4.0.0 and rel to dev8
* Mon Dec 31 2012 Peter Dunkley <peter@dunkley.me.uk>
  - Added dialog2 and IMS modules to the build
* Fri Dec 21 2012 Peter Dunkley <peter@dunkley.me.uk>
  - Added db2_ldap, db2_ops, and timer to the build
  - Added uid_auth_db, uid_avp_db, uid_domain, uid_gflags, uid_uri_db, print,
    and print_lib to the build
* Thu Dec 13 2012 Peter Dunkley <peter@dunkley.me.uk>
  - Added xhttp_pi framework examples to the installation
  - Added xhttp_pi README to the installation
* Wed Dec 12 2012 Peter Dunkley <peter@dunkley.me.uk>
  - Added mangler module to the build
  - Tidied up make commands used to build and install
* Sun Dec 9 2012 Peter Dunkley <peter@dunkley.me.uk>
  - Updated rel to dev7
  - Added avp, sca, and xprint modules to the build
  - Moved xlog from modules_k to modules
* Fri Nov 9 2012 Peter Dunkley <peter@dunkley.me.uk>
  - Updated rel to dev5
* Tue Oct 30 2012 Peter Dunkley <peter@dunkley.me.uk>
  - Added xhttp_pi module to RPM builds
* Fri Oct 20 2012 Peter Dunkley <peter@dunkley.me.uk>
  - Set ownership of /etc/kamailio to kamailio.kamailio
  - Added installation of auth.7.gz for Fedora now that manpages are built for
    Fedora
  - Added "make utils" to the build section (when it's not there utils get
    built during the install - which isn't right)
  - SCTP and STUN now included in this build
  - Removed kamailio-tls package - tls module now in main kamailio RPM as that
    has openssl as a dependency for STUN
* Sun Sep 17 2012 Peter Dunkley <peter@dunkley.me.uk>
  - Added corex module to RPM builds
  - Updated rel to dev4
* Sun Aug 19 2012 Peter Dunkley <peter@dunkley.me.uk>
  - Updated rel to dev3
* Mon Aug 13 2012 Peter Dunkley <peter@dunkley.me.uk>
  - Added Outbound module
* Fri Jul 13 2012 Peter Dunkley <peter@dunkley.me.uk>
  - Updated rel to dev2
* Thu Jul 5 2012 Peter Dunkley <peter@dunkley.me.uk>
  - Added kamailio-cdp RPM for cdp and cdp_avp modules
* Tue Jul 3 2012 Peter Dunkley <peter@dunkley.me.uk>
  - Updates to websocket module
* Sat Jun 30 2012 Peter Dunkley <peter@dunkley.me.uk>
  - Updated rel to dev1
  - Removed %_sharedir and replaced with standard macro %_datadir
* Sat Jun 23 2012 Peter Dunkley <peter@dunkley.me.uk>
  - Added websocket module
* Mon Jun 11 2012 Peter Dunkley <peter@dunkley.me.uk>
  - Updated ver to 3.4.0 and rel to dev0
* Mon Jun 4 2012 Peter Dunkley <peter@dunkley.me.uk>
  - Added a number of %dir lines to make sure the RPMs are properly cleaned up
    on uninstall
* Sat Jun 2 2012 Peter Dunkley <peter@dunkley.me.uk>
  - Added %postun section to reload systemd on Fedora after uninstall
  - Added build requirement for redhat-rpm-config so debuginfo RPMs get built
* Fri Jun 1 2012 Peter Dunkley <peter@dunkley.me.uk>
  - Tweak to the pkg/kamailio/fedora directory structure
  - Tested with Fedora 17
* Thu May 31 2012 Peter Dunkley <peter@dunkley.me.uk>
  - Updated rel to pre3
  - Combined Fedora/CentOS .spec in preparation for Fedora 17
* Sun May 20 2012 Peter Dunkley <peter@dunkley.me.uk>
  - First version created for Kamailio 3.3.0. Based on spec-file for Fedora
    created by myself (in turn based on an older spec-file for CentOS created
    by Ovidiu Sas).
  - Tested with CentOS 6.2 x86_64.
  - Builds all Kamailio 3.3.0 modules (modules/modules_k) except:
    - modules/app_mono: Requires mono which is not in the CentOS 6 repo
    - modules/auth_identity: Conflicts with TLS unless statically linked (which
      requires changes to Makefile and is impractical for generic RPM building)
    - modules/db_cassandra: Requires Cassandra and Thrift which are not in the
      CentOS 6 repo
    - modules/geoip: Requires GeoIP which is not in the CentOS 6 repo
    - modules/iptrtpproxy: Needs local copy of iptables source to build
      (impractical for generic RPM building)
    - modules/json: Requires json-c whish is not in the CentOS 6 repo
    - modules/jsonrpc-c: Requires json-c whish is not in the CentOS 6 repo
    - modules/ndb_redis: Requires hiredis which is not in the CentOS 6 repo
    - modules/peering: Requires radiusclient-ng which is not in the CentOS 6
      repo
    - modules_k/acc_radius: Requires radiusclient-ng which is not in the CentOS
      6 repo
    - modules_k/auth_radius: Required radiusclient-ng which is not in the
      CentOS 6 repo
    - modules_k/carrierroute: Requires libconfuse which is not in the CentOS 6
      repo
    - modules_k/db_oracle: Requires Oracle which is not in the CentOS 6 repo
      (and is closed-source)
    - modules_k/memcached: Module compilation appears to require an older
      version of libmemcached-devel than the one in the CentOS 6 repo
    - modules_k/mi_xmlrpc: Requires libxmlrpc-c3 which is not in the CentOS 6
      repo
    - modules_k/misc_radius: Requires radiusclient-ng which is not in the
      CentOS 6 repo
    - modules_k/osp: Requires OSP Toolkit which is not in the CentOS 6 repo
* Fri May 18 2012 Peter Dunkley <peter@dunkley.me.uk>
  - Added missing BuildRequires (gcc).
  - Added .fc16 to rel.  This makes it easy to tell which distribution the RPMs
    are built for.
* Thu May 17 2012 Peter Dunkley <peter@dunkley.me.uk>
  - Updated rel to pre2.
* Mon May 7 2012 Peter Dunkley <peter@dunkley.me.uk>
  - Changed to use systemd instead of SysV init.
* Sun May 6 2012 Peter Dunkley <peter@dunkley.me.uk>
  - First version created for Kamailio 3.3.0. Based on spec-file for CentOS
    created by Ovidiu Sas.
  - Tested with Fedora 16 x86_64.
  - Builds all Kamailio 3.3.0 modules (modules/modules_k) except:
    - modules/auth_identity: Conflicts with TLS unless statically linked (which
      requires changes to Makefile and is impractical for generic RPM building)
    - modules/db_cassandra: Requires Thrift which is not in the F16 repo
    - modules/iptrtpproxy: Needs local copy of iptables source to build
      (impractical for generic RPM building)
    - modules_k/db_oracle: Requires Oracle which is not in the F16 repo
      (and is closed-source)
    - modules_k/memcached: Module compilation appears to require an older
      version of libmemcached-devel than the one in the F16 repo
    - modules_k/mi_xmlrpc: The F16 repo contains an unsupported version of
      libxmlrpc-c3, and there is an compilation error due to the module code
      using an unknown type ('TString')
    - modules_k/osp: Requires OSP Toolkit which is not in the F16 repo
