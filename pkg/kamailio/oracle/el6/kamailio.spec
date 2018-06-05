%define name    kamailio
%define ver     5.1.4
%define rel     0
%define _sharedir %{_prefix}/share

%define MYSQL_MODULES           mysql
%define POSTGRES_MODULES        postgres
%define UNIXODBC_MODULES        unixodbc
%define LDAP_MODULES            ldap
%define XMLRPC_MODULES          xml
%define PERL_MODULES            perl
%define PYTHON_MODULES          python
%define LUA_MODULES             lua
%define UTILS_MODULES           utils
%define PURPLE_MODULES          purple
%define MEMCACHED_MODULES       memcached
%define TLS_MODULES             tls
%define XMPP_MODULES            xmpp
%define CPL_MODULES             cpl
%define SNMPSTATS_MODULES       snmpstats
%define CARRIERROUTE_MODULES    carrierroute
%define PRESENCE_MODULES        presence
%define RADIUS_MODULES          radius
%define GEOIP_MODULES           geoip

Summary:      Kamailio, very fast and flexible SIP Server
Name:         %name
Version:      %ver
Release:      %rel
Packager:     Ovidiu Sas <osas@voipembedded.com>
License:      GPL
Group:        System Environment/Daemons
Source0:      http://kamailio.org/pub/kamailio/%{ver}/%{name}-%{ver}_src.tar.gz
Source1:      kamailio.init
Source2:      kamailio.default
URL:          http://kamailio.org/
Vendor:       kamailio.org
BuildRoot:    %{_tmppath}/%{name}-%{ver}-buildroot
Conflicts:    kamailio-mysql < %ver, kamailio-postgres < %ver, kamailio-unixodbc < %ver, kamailio-ldap < %ver, kamailio-xmlrpc < %ver, kamailio-perl < %ver, kamailio-python < %ver, kamailio-lua < %ver, kamailio-utils < %ver, kamailio-purple < %ver, kamailio-memcached  < %ver, kamailio-tls  < %ver, kamailio-xmpp  < %ver, kamailio-cpl  < %ver, kamailio-snmpstats  < %ver, kamailio-carrierroute  < %ver, kamailio-presence  < %ver, kamailio-radius  < %ver, kamailio-geoip  < %ver
Requires:     shadow-utils
BuildRequires:  make flex bison pcre-devel

%description
Kamailio is a very fast and flexible SIP (RFC3261)
proxy server. Written entirely in C, kamailio can handle thousands calls
per second even on low-budget hardware. A C Shell like scripting language
provides full control over the server's behaviour. It's modular
architecture allows only required functionality to be loaded.
Currently the following modules are available: digest authentication,
CPL scripts, instant messaging, MySQL and UNIXODBC support, a presence agent,
radius authentication, record routing, an SMS gateway, a
transaction and dialog module, OSP module, statistics support,
registrar and user location, SNMP, SIMPLE Presence and Perl programming
interface.

%package  mysql
Summary:  MySQL connectivity for the Kamailio.
Group:    System Environment/Daemons
Requires: kamailio = %ver
BuildRequires:  mysql-devel zlib-devel

%description mysql
The kamailio-mysql package contains MySQL database connectivity that you
need to use digest authentication module or persistent user location
entries.


%package  postgres
Summary:  MPOSTGRES connectivity for the Kamailio.
Group:    System Environment/Daemons
Requires: kamailio = %ver
BuildRequires:  postgresql-devel

%description postgres
The kamailio-postgres package contains Postgres database connectivity that you
need to use digest authentication module or persistent user location
entries.


%package  unixodbc
Summary:  UNIXODBC connectivity for Kamailio.
Group:    System Environment/Daemons
Requires: kamailio = %ver
BuildRequires:  unixODBC-devel

%description unixodbc
The kamailio-unixodbc package contains UNIXODBC database connectivity support
that is required by other modules with database dependencies.


%package  utils
Summary:  Utils for Kamailio.
Group:    System Environment/Daemons
Requires: kamailio = %ver

%description utils
The kamailio-utils package provides a set utility functions for Kamailio


%package  cpl
Summary:  CPL module (CPL interpreter engine) for Kamailio
Group:    System Environment/Daemons
Requires: kamailio = %ver
BuildRequires:  libxml2-devel

%description cpl
The kamailio-cpl package provides a CPL interpreter engine for Kamailio


%package  radius
Summary:  Kamailio radius support for AAA API.
Group:    System Environment/Daemons
Requires: kamailio = %ver
BuildRequires:  radiusclient-ng-devel

%description radius
The kamailio-radius package contains modules for radius authentication, group
membership and uri checking.


%package  snmpstats
Summary:  SNMP AgentX subagent module for Kamailio
Group:    System Environment/Daemons
Requires: kamailio = %ver, net-snmp-utils
BuildRequires:  lm_sensors-devel net-snmp-devel

%description snmpstats
The kamailio-snmpstats package snmpstats module for Kamailio.  This module acts
as an AgentX subagent which connects to a master agent.


%package  presence
Summary:  sip presence user agent support for Kamailio
Group:    System Environment/Daemons
Requires: kamailio = %ver
BuildRequires:  libxml2-devel, curl-devel

%description presence
The kamailio-presence package contains a sip Presence Agent.


%package  xmpp
Summary:  SIP2XMPP message translation support for Kamailio.
Group:    System Environment/Daemons
Requires: kamailio = %ver
BuildRequires:  expat-devel

%description xmpp
The kamailio-xmpp package contains a SIP to XMPP message translator.


%package  tls
Summary:  TLS transport protocol for Kamailio.
Group:    System Environment/Daemons
Requires: kamailio = %ver
BuildRequires:  openssl-devel

%description tls
The kamailio-tls package contains the SIP TLSt transport mechanism for Kamailio.


%package  carrierroute
Summary:  Routing module for Kamailio.
Group:    System Environment/Daemons
Requires: kamailio = %ver
BuildRequires:  libconfuse-devel

%description carrierroute
The kamailio-carrierroute package contains a fast routing engine.


%package  purple
Summary:  Provides the purple module, a multi-protocol IM gateway.
Group:    System Environment/Daemons
Requires: kamailio = %ver
BuildRequires: libpurple-devel

%description purple
The kamailio-purple package provides the purple module, a multi-protocol instant
messaging gateway module.


%package  ldap
Summary:  LDAP modules for Kamailio.
Group:    System Environment/Daemons
Requires: kamailio = %ver
BuildRequires: openldap-devel

%description ldap
The kamailio-ldap package provides the ldap and h350 modules for Kamailio,
enabling LDAP queries from the Kamailio config and storage of SIP account
data in an LDAP directory.


#%package  memcached
#Summary:  Distributed hash table for Kamailio.
#Group:    System Environment/Daemons
#Requires: kamailio = %ver
#BuildRequires:  libmemcached-devel
#
#%description memcached
#The kamailio-memcached package provides access to a distributed hash table memcached.


#%package  xmlrpc
#Summary:  XMLRPC support for Kamailio's Management Interface.
#Group:    System Environment/Daemons
#Requires: kamailio = %ver
#BuildRequires:  libxml2-devel xmlrpc-c-devel
#
#%description xmlrpc
#The kamailio-xmlrpc package provides the XMLRPC transport implementations for Kamailio's
#Management and Control Interface.


%package  perl
Summary:  Perl extensions and database driver for Kamailio.
Group:    System Environment/Daemons 
Requires: kamailio = %ver
BuildRequires: mod_perl-devel

%description perl
The kamailio-perl package provides an interface for Kamailio to write Perl extensions and
the perlvdb database driver for Kamailio.


%package  lua
Summary:  Lua extensions for Kamailio.
Group:    System Environment/Daemons
Requires: kamailio = %ver
BuildRequires: lua-devel

%description lua
The kamailio-lua package provides an interface for Kamailio to write Python extensions


%package  python
Summary:  Python extensions for Kamailio.
Group:    System Environment/Daemons
Requires: kamailio = %ver
BuildRequires: python-devel

%description python
The kamailio-python package provides an interface for Kamailio to write Python extensions


%package  geoip
Summary:  GeoIP extensions for Kamailio.
Group:    System Environment/Daemons
Requires: kamailio = %ver
BuildRequires: geoip-devel

%description geoip
The kamailio-geoip package provides a GeoIP interface for Kamailio




%prep
%setup -n %{name}-%{ver}

%build
make FLAVOUR=kamailio cfg prefix=/usr cfg_prefix=$RPM_BUILD_ROOT basedir=$RPM_BUILD_ROOT cfg_target=/%{_sysconfdir}/kamailio/ modules_dirs="modules"
make
make every-module skip_modules="iptrtpproxy" group_include="kstandard"
make every-module group_include="k%MYSQL_MODULES"
make every-module group_include="k%POSTGRES_MODULES"
make every-module group_include="k%UNIXODBC_MODULES"
make every-module group_include="k%UTILS_MODULES"
make every-module group_include="k%CPL_MODULES"
make every-module group_include="k%RADIUS_MODULES"
make every-module group_include="k%SNMPSTATS_MODULES"
make every-module group_include="k%PRESENCE_MODULES"
make every-module group_include="k%XMPP_MODULES"
make every-module group_include="k%TLS_MODULES"
make every-module group_include="k%CARRIERROUTE_MODULES"
make every-module group_include="k%PURPLE_MODULES"
make every-module group_include="k%LDAP_MODULES"
#make every-module group_include="k%MEMCACHED_MODULES"
#make every-module group_include="k%XMLRPC_MODULES"
make every-module group_include="k%PERL_MODULES"
make every-module group_include="k%LUA_MODULES"
make every-module group_include="k%PYTHON_MODULES"
make every-module group_include="k%GEOIP_MODULES"

%install
[ "$RPM_BUILD_ROOT" != "/" ] && rm -rf "$RPM_BUILD_ROOT"

make install
make install-modules-all skip_modules="iptrtpproxy" group_include="kstandard"
make install-modules-all group_include="k%MYSQL_MODULES"
make install-modules-all group_include="k%POSTGRES_MODULES"
make install-modules-all group_include="k%UNIXODBC_MODULES"
make install-modules-all group_include="k%UTILS_MODULES"
make install-modules-all group_include="k%CPL_MODULES"
make install-modules-all group_include="k%SNMPSTATS_MODULES"
make install-modules-all group_include="k%RADIUS_MODULES"
make install-modules-all group_include="k%PRESENCE_MODULES"
make install-modules-all group_include="k%XMPP_MODULES"
make install-modules-all group_include="k%TLS_MODULES"
make install-modules-all group_include="k%CARRIERROUTE_MODULES"
make install-modules-all group_include="k%PURPLE_MODULES"
make install-modules-all group_include="k%LDAP_MODULES"
#make install-modules-all group_include="k%MEMCACHED_MODULES"
#make install-modules-all group_include="k%XMLRPC_MODULES"
make install-modules-all group_include="k%PERL_MODULES"
make install-modules-all group_include="k%LUA_MODULES"
make install-modules-all group_include="k%PYTHON_MODULES"
make install-modules-all group_include="k%GEOIP_MODULES"


mkdir -p $RPM_BUILD_ROOT/%{_sysconfdir}/rc.d/init.d
install -m755 $RPM_SOURCE_DIR/kamailio.init \
              $RPM_BUILD_ROOT/%{_sysconfdir}/rc.d/init.d/kamailio

mkdir -p $RPM_BUILD_ROOT/%{_sysconfdir}/default
install -m755 $RPM_SOURCE_DIR/kamailio.default \
              $RPM_BUILD_ROOT/%{_sysconfdir}/default/kamailio


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
%doc %{_docdir}/kamailio/modules/README.auth
%doc %{_docdir}/kamailio/modules/README.avpops
%doc %{_docdir}/kamailio/modules/README.blst
%doc %{_docdir}/kamailio/modules/README.cfg_db
%doc %{_docdir}/kamailio/modules/README.cfg_rpc
%doc %{_docdir}/kamailio/modules/README.counters
%doc %{_docdir}/kamailio/modules/README.ctl
%doc %{_docdir}/kamailio/modules/README.db_flatstore
%doc %{_docdir}/kamailio/modules/README.debugger
%doc %{_docdir}/kamailio/modules/README.dialplan
%doc %{_docdir}/kamailio/modules/README.enum
%doc %{_docdir}/kamailio/modules/README.lcr
%doc %{_docdir}/kamailio/modules/README.malloc_test
%doc %{_docdir}/kamailio/modules/README.matrix
%doc %{_docdir}/kamailio/modules/README.mediaproxy
%doc %{_docdir}/kamailio/modules/README.mi_rpc
%doc %{_docdir}/kamailio/modules/README.mqueue
%doc %{_docdir}/kamailio/modules/README.mtree
%doc %{_docdir}/kamailio/modules/README.pdb
%doc %{_docdir}/kamailio/modules/README.pipelimit
%doc %{_docdir}/kamailio/modules/README.prefix_route
#%doc %{_docdir}/kamailio/modules/README.privacy
%doc %{_docdir}/kamailio/modules/README.ratelimit
%doc %{_docdir}/kamailio/modules/README.sanity
%doc %{_docdir}/kamailio/modules/README.sl
%doc %{_docdir}/kamailio/modules/README.sms
%doc %{_docdir}/kamailio/modules/README.textopsx
%doc %{_docdir}/kamailio/modules/README.tm
%doc %{_docdir}/kamailio/modules/README.topoh
%doc %{_docdir}/kamailio/modules/README.xhttp
%doc %{_docdir}/kamailio/modules/README.acc
%doc %{_docdir}/kamailio/modules/README.alias_db
%doc %{_docdir}/kamailio/modules/README.auth_db
%doc %{_docdir}/kamailio/modules/README.auth_diameter
%doc %{_docdir}/kamailio/modules/README.benchmark
%doc %{_docdir}/kamailio/modules/README.call_control
%doc %{_docdir}/kamailio/modules/README.cfgutils
%doc %{_docdir}/kamailio/modules/README.db_text
%doc %{_docdir}/kamailio/modules/README.dialog
%doc %{_docdir}/kamailio/modules/README.dispatcher
%doc %{_docdir}/kamailio/modules/README.diversion
%doc %{_docdir}/kamailio/modules/README.domain
%doc %{_docdir}/kamailio/modules/README.domainpolicy
%doc %{_docdir}/kamailio/modules/README.drouting
%doc %{_docdir}/kamailio/modules/README.exec
%doc %{_docdir}/kamailio/modules/README.group
%doc %{_docdir}/kamailio/modules/README.htable
%doc %{_docdir}/kamailio/modules/README.imc
%doc %{_docdir}/kamailio/modules/README.kex
%doc %{_docdir}/kamailio/modules/README.maxfwd
%doc %{_docdir}/kamailio/modules/README.mi_datagram
%doc %{_docdir}/kamailio/modules/README.mi_fifo
%doc %{_docdir}/kamailio/modules/README.msilo
%doc %{_docdir}/kamailio/modules/README.nat_traversal
%doc %{_docdir}/kamailio/modules/README.nathelper
%doc %{_docdir}/kamailio/modules/README.path
%doc %{_docdir}/kamailio/modules/README.pdt
%doc %{_docdir}/kamailio/modules/README.permissions
%doc %{_docdir}/kamailio/modules/README.pike
%doc %{_docdir}/kamailio/modules/README.pua_mi
%doc %{_docdir}/kamailio/modules/README.pv
%doc %{_docdir}/kamailio/modules/README.qos
%doc %{_docdir}/kamailio/modules/README.regex
%doc %{_docdir}/kamailio/modules/README.registrar
%doc %{_docdir}/kamailio/modules/README.rr
%doc %{_docdir}/kamailio/modules/README.rtimer
%doc %{_docdir}/kamailio/modules/README.rtpproxy
%doc %{_docdir}/kamailio/modules/README.seas
%doc %{_docdir}/kamailio/modules/README.siptrace
%doc %{_docdir}/kamailio/modules/README.siputils
%doc %{_docdir}/kamailio/modules/README.speeddial
%doc %{_docdir}/kamailio/modules/README.sqlops
%doc %{_docdir}/kamailio/modules/README.sst
%doc %{_docdir}/kamailio/modules/README.statistics
%doc %{_docdir}/kamailio/modules/README.textops
%doc %{_docdir}/kamailio/modules/README.tmx
%doc %{_docdir}/kamailio/modules/README.uac
%doc %{_docdir}/kamailio/modules/README.uac_redirect
%doc %{_docdir}/kamailio/modules/README.uri_db
%doc %{_docdir}/kamailio/modules/README.userblacklist
%doc %{_docdir}/kamailio/modules/README.usrloc
%doc %{_docdir}/kamailio/modules/README.xlog

%doc %{_docdir}/kamailio/modules/README.app_perl
%doc %{_docdir}/kamailio/modules/README.async
%doc %{_docdir}/kamailio/modules/README.auth_identity
%doc %{_docdir}/kamailio/modules/README.auth_xkeys
%doc %{_docdir}/kamailio/modules/README.avp
%doc %{_docdir}/kamailio/modules/README.cfgt
%doc %{_docdir}/kamailio/modules/README.corex
%doc %{_docdir}/kamailio/modules/README.crypto
%doc %{_docdir}/kamailio/modules/README.db2_ldap
%doc %{_docdir}/kamailio/modules/README.db2_ops
%doc %{_docdir}/kamailio/modules/README.db_cluster
%doc %{_docdir}/kamailio/modules/README.db_mysql
%doc %{_docdir}/kamailio/modules/README.db_perlvdb
%doc %{_docdir}/kamailio/modules/README.dmq
%doc %{_docdir}/kamailio/modules/README.dmq_usrloc
%doc %{_docdir}/kamailio/modules/README.http_client
%doc %{_docdir}/kamailio/modules/README.ipops
%doc %{_docdir}/kamailio/modules/README.jsonrpc-s
%doc %{_docdir}/kamailio/modules/README.log_custom
%doc %{_docdir}/kamailio/modules/README.mangler
%doc %{_docdir}/kamailio/modules/README.mohqueue
%doc %{_docdir}/kamailio/modules/README.msrp
%doc %{_docdir}/kamailio/modules/README.nosip
%doc %{_docdir}/kamailio/modules/README.p_usrloc
%doc %{_docdir}/kamailio/modules/README.presence_profile
%doc %{_docdir}/kamailio/modules/README.presence_reginfo
%doc %{_docdir}/kamailio/modules/README.print
%doc %{_docdir}/kamailio/modules/README.print_lib
%doc %{_docdir}/kamailio/modules/README.pua_reginfo
%doc %{_docdir}/kamailio/modules/README.rtjson
%doc %{_docdir}/kamailio/modules/README.rtpengine
%doc %{_docdir}/kamailio/modules/README.sca
%doc %{_docdir}/kamailio/modules/README.sdpops
%doc %{_docdir}/kamailio/modules/README.sipcapture
%doc %{_docdir}/kamailio/modules/README.sipt
%doc %{_docdir}/kamailio/modules/README.smsops
%doc %{_docdir}/kamailio/modules/README.statsc
%doc %{_docdir}/kamailio/modules/README.statsd
%doc %{_docdir}/kamailio/modules/README.stun
%doc %{_docdir}/kamailio/modules/README.tcpops
%doc %{_docdir}/kamailio/modules/README.timer
%doc %{_docdir}/kamailio/modules/README.tmrec
%doc %{_docdir}/kamailio/modules/README.topos
%doc %{_docdir}/kamailio/modules/README.tsilo
%doc %{_docdir}/kamailio/modules/README.uid_auth_db
%doc %{_docdir}/kamailio/modules/README.uid_avp_db
%doc %{_docdir}/kamailio/modules/README.uid_domain
%doc %{_docdir}/kamailio/modules/README.uid_gflags
%doc %{_docdir}/kamailio/modules/README.uid_uri_db
%doc %{_docdir}/kamailio/modules/README.xhttp_rpc
%doc %{_docdir}/kamailio/modules/README.xprint

%{_datarootdir}/snmp/mibs/KAMAILIO-MIB
%{_datarootdir}/snmp/mibs/KAMAILIO-REG-MIB
%{_datarootdir}/snmp/mibs/KAMAILIO-SIP-COMMON-MIB
%{_datarootdir}/snmp/mibs/KAMAILIO-SIP-SERVER-MIB
%{_datarootdir}/snmp/mibs/KAMAILIO-TC

%dir %{_sysconfdir}/kamailio
%config(noreplace) %{_sysconfdir}/kamailio/*
%config %{_sysconfdir}/rc.d/init.d/*
%config %{_sysconfdir}/default/*

%dir %{_libdir}/kamailio
%{_libdir}/kamailio/libbinrpc.so
%{_libdir}/kamailio/libbinrpc.so.0
%{_libdir}/kamailio/libbinrpc.so.0.1
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
%{_libdir}/kamailio/modules/auth.so
%{_libdir}/kamailio/modules/avpops.so
%{_libdir}/kamailio/modules/blst.so
%{_libdir}/kamailio/modules/cfg_db.so
%{_libdir}/kamailio/modules/cfg_rpc.so
%{_libdir}/kamailio/modules/counters.so
%{_libdir}/kamailio/modules/ctl.so
%{_libdir}/kamailio/modules/db_flatstore.so
%{_libdir}/kamailio/modules/debugger.so
%{_libdir}/kamailio/modules/dialplan.so
%{_libdir}/kamailio/modules/enum.so
%{_libdir}/kamailio/modules/lcr.so
%{_libdir}/kamailio/modules/malloc_test.so
%{_libdir}/kamailio/modules/matrix.so
%{_libdir}/kamailio/modules/mediaproxy.so
%{_libdir}/kamailio/modules/mi_rpc.so
%{_libdir}/kamailio/modules/mqueue.so
%{_libdir}/kamailio/modules/mtree.so
%{_libdir}/kamailio/modules/pdb.so
%{_libdir}/kamailio/modules/pipelimit.so
%{_libdir}/kamailio/modules/prefix_route.so
#%{_libdir}/kamailio/modules/privacy.so
%{_libdir}/kamailio/modules/ratelimit.so
%{_libdir}/kamailio/modules/sanity.so
%{_libdir}/kamailio/modules/sl.so
%{_libdir}/kamailio/modules/sms.so
%{_libdir}/kamailio/modules/tm.so
%{_libdir}/kamailio/modules/textopsx.so
%{_libdir}/kamailio/modules/topoh.so
%{_libdir}/kamailio/modules/xhttp.so
%{_libdir}/kamailio/modules/acc.so
%{_libdir}/kamailio/modules/alias_db.so
%{_libdir}/kamailio/modules/auth_db.so
%{_libdir}/kamailio/modules/auth_diameter.so
%{_libdir}/kamailio/modules/benchmark.so
%{_libdir}/kamailio/modules/call_control.so
%{_libdir}/kamailio/modules/cfgutils.so
%{_libdir}/kamailio/modules/db_text.so
%{_libdir}/kamailio/modules/dialog.so
%{_libdir}/kamailio/modules/dispatcher.so
%{_libdir}/kamailio/modules/diversion.so
%{_libdir}/kamailio/modules/domain.so
%{_libdir}/kamailio/modules/domainpolicy.so
%{_libdir}/kamailio/modules/drouting.so
%{_libdir}/kamailio/modules/exec.so
%{_libdir}/kamailio/modules/group.so
%{_libdir}/kamailio/modules/htable.so
%{_libdir}/kamailio/modules/imc.so
%{_libdir}/kamailio/modules/kex.so
%{_libdir}/kamailio/modules/maxfwd.so
%{_libdir}/kamailio/modules/mi_datagram.so
%{_libdir}/kamailio/modules/mi_fifo.so
%{_libdir}/kamailio/modules/msilo.so
%{_libdir}/kamailio/modules/nat_traversal.so
%{_libdir}/kamailio/modules/nathelper.so
%{_libdir}/kamailio/modules/path.so
%{_libdir}/kamailio/modules/pdt.so
%{_libdir}/kamailio/modules/permissions.so
%{_libdir}/kamailio/modules/pike.so
%{_libdir}/kamailio/modules/pua_mi.so
%{_libdir}/kamailio/modules/pv.so
%{_libdir}/kamailio/modules/qos.so
%{_libdir}/kamailio/modules/regex.so
%{_libdir}/kamailio/modules/registrar.so
%{_libdir}/kamailio/modules/rr.so
%{_libdir}/kamailio/modules/rtimer.so
%{_libdir}/kamailio/modules/rtpproxy.so
%{_libdir}/kamailio/modules/seas.so
%{_libdir}/kamailio/modules/siptrace.so
%{_libdir}/kamailio/modules/siputils.so
%{_libdir}/kamailio/modules/speeddial.so
%{_libdir}/kamailio/modules/sqlops.so
%{_libdir}/kamailio/modules/sst.so
%{_libdir}/kamailio/modules/statistics.so
%{_libdir}/kamailio/modules/textops.so
%{_libdir}/kamailio/modules/tmx.so
%{_libdir}/kamailio/modules/uac.so
%{_libdir}/kamailio/modules/uac_redirect.so
%{_libdir}/kamailio/modules/uri_db.so
%{_libdir}/kamailio/modules/userblacklist.so
%{_libdir}/kamailio/modules/usrloc.so
%{_libdir}/kamailio/modules/xlog.so
%{_libdir}/kamailio/modules/async.so
%{_libdir}/kamailio/modules/auth_identity.so
%{_libdir}/kamailio/modules/auth_xkeys.so
%{_libdir}/kamailio/modules/avp.so
%{_libdir}/kamailio/modules/cfgt.so
%{_libdir}/kamailio/modules/corex.so
%{_libdir}/kamailio/modules/crypto.so
%{_libdir}/kamailio/modules/db2_ldap.so
%{_libdir}/kamailio/modules/db2_ops.so
%{_libdir}/kamailio/modules/db_cluster.so
%{_libdir}/kamailio/modules/dmq.so
%{_libdir}/kamailio/modules/dmq_usrloc.so
%{_libdir}/kamailio/modules/http_client.so
%{_libdir}/kamailio/modules/ipops.so
%{_libdir}/kamailio/modules/jsonrpc-s.so
%{_libdir}/kamailio/modules/log_custom.so
%{_libdir}/kamailio/modules/mangler.so
%{_libdir}/kamailio/modules/mohqueue.so
%{_libdir}/kamailio/modules/msrp.so
%{_libdir}/kamailio/modules/nosip.so
%{_libdir}/kamailio/modules/p_usrloc.so
%{_libdir}/kamailio/modules/presence_profile.so
%{_libdir}/kamailio/modules/presence_reginfo.so
%{_libdir}/kamailio/modules/print.so
%{_libdir}/kamailio/modules/print_lib.so
%{_libdir}/kamailio/modules/pua_reginfo.so
%{_libdir}/kamailio/modules/rtjson.so
%{_libdir}/kamailio/modules/rtpengine.so
%{_libdir}/kamailio/modules/sca.so
%{_libdir}/kamailio/modules/sdpops.so
%{_libdir}/kamailio/modules/sipcapture.so
%{_libdir}/kamailio/modules/sipt.so
%{_libdir}/kamailio/modules/smsops.so
%{_libdir}/kamailio/modules/statsc.so
%{_libdir}/kamailio/modules/statsd.so
%{_libdir}/kamailio/modules/stun.so
%{_libdir}/kamailio/modules/tcpops.so
%{_libdir}/kamailio/modules/timer.so
%{_libdir}/kamailio/modules/tmrec.so
%{_libdir}/kamailio/modules/topos.so
%{_libdir}/kamailio/modules/tsilo.so
%{_libdir}/kamailio/modules/uid_auth_db.so
%{_libdir}/kamailio/modules/uid_avp_db.so
%{_libdir}/kamailio/modules/uid_domain.so
%{_libdir}/kamailio/modules/uid_gflags.so
%{_libdir}/kamailio/modules/uid_uri_db.so
%{_libdir}/kamailio/modules/xhttp_rpc.so
%{_libdir}/kamailio/modules/xprint.so


%{_sbindir}/kamailio
%{_sbindir}/kamctl
%{_sbindir}/kamdbctl
%{_sbindir}/kamcmd
%{_libdir}/kamailio/kamctl/dbtextdb/dbtextdb.py
%{_libdir}/kamailio/kamctl/dbtextdb/dbtextdb.pyc
%{_libdir}/kamailio/kamctl/dbtextdb/dbtextdb.pyo
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

%{_mandir}/man5/*
%{_mandir}/man8/*

%{_sharedir}/kamailio/dbtext/kamailio/*


%files mysql
%defattr(-,root,root)
%{_libdir}/kamailio/modules/db_mysql.so
%{_libdir}/kamailio/kamctl/kamctl.mysql
%{_libdir}/kamailio/kamctl/kamdbctl.mysql
%{_sharedir}/kamailio/mysql/*


%files postgres
%defattr(-,root,root)
%doc %{_docdir}/kamailio/modules/README.db_postgres
%{_libdir}/kamailio/modules/db_postgres.so
%{_libdir}/kamailio/kamctl/kamctl.pgsql
%{_libdir}/kamailio/kamctl/kamdbctl.pgsql
%{_sharedir}/kamailio/postgres/*


%files unixodbc
%defattr(-,root,root)
%doc %{_docdir}/kamailio/modules/README.db_unixodbc
%{_libdir}/kamailio/modules/db_unixodbc.so


%files utils
%defattr(-,root,root)
%{_docdir}/kamailio/modules/README.utils
%{_libdir}/kamailio/modules/utils.so


%files cpl
%defattr(-,root,root)
%{_docdir}/kamailio/modules/README.cpl-c
%{_libdir}/kamailio/modules/cpl-c.so


%files radius
%defattr(-,root,root)
%{_docdir}/kamailio/modules/README.acc_radius
%{_docdir}/kamailio/modules/README.auth_radius
%{_docdir}/kamailio/modules/README.misc_radius
%{_docdir}/kamailio/modules/README.peering
%{_libdir}/kamailio/modules/acc_radius.so
%{_libdir}/kamailio/modules/auth_radius.so
%{_libdir}/kamailio/modules/misc_radius.so
%{_libdir}/kamailio/modules/peering.so


%files snmpstats
%defattr(-,root,root)
%{_docdir}/kamailio/modules/README.snmpstats
%{_libdir}/kamailio/modules/snmpstats.so


%files presence
%defattr(-,root,root)
%doc %{_docdir}/kamailio/modules/README.presence
%doc %{_docdir}/kamailio/modules/README.presence_conference
%doc %{_docdir}/kamailio/modules/README.presence_dialoginfo
%doc %{_docdir}/kamailio/modules/README.presence_mwi
%doc %{_docdir}/kamailio/modules/README.presence_xml
%doc %{_docdir}/kamailio/modules/README.pua
%doc %{_docdir}/kamailio/modules/README.pua_bla
%doc %{_docdir}/kamailio/modules/README.pua_dialoginfo
%doc %{_docdir}/kamailio/modules/README.pua_mi
%doc %{_docdir}/kamailio/modules/README.pua_usrloc
%doc %{_docdir}/kamailio/modules/README.pua_xmpp
%doc %{_docdir}/kamailio/modules/README.rls
%doc %{_docdir}/kamailio/modules/README.xcap_client
%doc %{_docdir}/kamailio/modules/README.xcap_server
%{_libdir}/kamailio/modules/presence.so
%{_libdir}/kamailio/modules/presence_conference.so
%{_libdir}/kamailio/modules/presence_dialoginfo.so
%{_libdir}/kamailio/modules/presence_mwi.so
%{_libdir}/kamailio/modules/presence_xml.so
%{_libdir}/kamailio/modules/pua.so
%{_libdir}/kamailio/modules/pua_bla.so
%{_libdir}/kamailio/modules/pua_dialoginfo.so
%{_libdir}/kamailio/modules/pua_mi.so
%{_libdir}/kamailio/modules/pua_usrloc.so
%{_libdir}/kamailio/modules/pua_xmpp.so
%{_libdir}/kamailio/modules/rls.so
%{_libdir}/kamailio/modules/xcap_client.so
%{_libdir}/kamailio/modules/xcap_server.so


%files xmpp
%defattr(-,root,root)
%doc %{_docdir}/kamailio/modules/README.xmpp
%{_libdir}/kamailio/modules/xmpp.so


%files tls
%defattr(-,root,root)
%doc %{_docdir}/kamailio/modules/README.tls
%{_libdir}/kamailio/modules/tls.so


%files carrierroute
%defattr(-,root,root)
%doc %{_docdir}/kamailio/modules/README.carrierroute
%{_libdir}/kamailio/modules/carrierroute.so


%files purple
%defattr(-,root,root)
%doc %{_docdir}/kamailio/modules/README.purple
%{_libdir}/kamailio/modules/purple.so


%files ldap
%defattr(-,root,root)
%doc %{_docdir}/kamailio/modules/README.h350
%doc %{_docdir}/kamailio/modules/README.ldap
%{_libdir}/kamailio/modules/h350.so
%{_libdir}/kamailio/modules/ldap.so


#%files memcached
#%defattr(-,root,root)
#%doc %{_docdir}/kamailio/modules/README.memcached
#%{_libdir}/kamailio/modules/memcached.so


#%files xmlrpc
#%defattr(-,root,root)
#%doc %{_docdir}/kamailio/modules/README.memcached
#%{_libdir}/kamailio/modules/memcached.so


%files perl
%defattr(-,root,root)
%{_libdir}/kamailio/modules/app_perl.so
%{_libdir}/kamailio/modules/db_perlvdb.so
%{_libdir}/kamailio/perl/Kamailio.pm
%{_libdir}/kamailio/perl/Kamailio/Constants.pm
%{_libdir}/kamailio/perl/Kamailio/LDAPUtils/LDAPConf.pm
%{_libdir}/kamailio/perl/Kamailio/LDAPUtils/LDAPConnection.pm
%{_libdir}/kamailio/perl/Kamailio/Message.pm
%{_libdir}/kamailio/perl/Kamailio/Utils/Debug.pm
%{_libdir}/kamailio/perl/Kamailio/Utils/PhoneNumbers.pm
%{_libdir}/kamailio/perl/Kamailio/VDB.pm
%{_libdir}/kamailio/perl/Kamailio/VDB/Adapter/AccountingSIPtrace.pm
%{_libdir}/kamailio/perl/Kamailio/VDB/Adapter/Alias.pm
%{_libdir}/kamailio/perl/Kamailio/VDB/Adapter/Auth.pm
%{_libdir}/kamailio/perl/Kamailio/VDB/Adapter/Describe.pm
%{_libdir}/kamailio/perl/Kamailio/VDB/Adapter/Speeddial.pm
%{_libdir}/kamailio/perl/Kamailio/VDB/Adapter/TableVersions.pm
%{_libdir}/kamailio/perl/Kamailio/VDB/Column.pm
%{_libdir}/kamailio/perl/Kamailio/VDB/Pair.pm
%{_libdir}/kamailio/perl/Kamailio/VDB/ReqCond.pm
%{_libdir}/kamailio/perl/Kamailio/VDB/Result.pm
%{_libdir}/kamailio/perl/Kamailio/VDB/VTab.pm
%{_libdir}/kamailio/perl/Kamailio/VDB/Value.pm

%files lua
%defattr(-,root,root)
%doc %{_docdir}/kamailio/modules/README.app_lua
%{_libdir}/kamailio/modules/app_lua.so


%files python
%defattr(-,root,root)
%doc %{_docdir}/kamailio/modules/README.app_python
%{_libdir}/kamailio/modules/app_python.so


%files geoip
%defattr(-,root,root)
%doc %{_docdir}/kamailio/modules/README.geoip
%{_libdir}/kamailio/modules/geoip.so



%changelog
* Mon Oct 4 2010 Ovidiu Sas <osas@voipembedded.com>
 - Update for kamailio 3.1

* Tue Mar 23 2010 Ovidiu Sas <osas@voipembedded.com>
 - First version of the spec file for kamailio 3.0
