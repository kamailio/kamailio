# $OpenBSD$

COMMENT-main =		mature and flexible open source SIP server
COMMENT-postgresql =	kamailio postgresql module
COMMENT-mysql =		kamailio mysql module
COMMENT-radius =	kamailio RADIUS modules
COMMENT-berkeleydb =	kamailio berkeley DB module
COMMENT-ldap =		kamailio LDAP modules
COMMENT-xmlrpc =	kamailio XML-RPC module
COMMENT-xcap_client =	kamailio XCAP client module
COMMENT-carrierroute =	kamailio carrierroute module
COMMENT-snmpstats =	kamailio snmpstats module
COMMENT-perl =		kamailio perl modules

VERSION =		1.5.0
DISTNAME =		kamailio-${VERSION}
PKGNAME-main =		kamailio-${VERSION}
PKGNAME-mysql =		kamailio-mysql-${VERSION}
PKGNAME-postgresql =	kamailio-postgresql-${VERSION}
PKGNAME-radius =	kamailio-radius-${VERSION}
PKGNAME-berkeleydb =	kamailio-berkeleydb-${VERSION}
PKGNAME-ldap =		kamailio-ldap-${VERSION}
PKGNAME-xmlrpc =	kamailio-xmlrpc-${VERSION}
PKGNAME-xcap_client =	kamailio-xcap_client-${VERSION}
PKGNAME-carrierroute =	kamailio-carrierroute-${VERSION}
PKGNAME-snmpstats =	kamailio-snmpstats-${VERSION}
PKGNAME-perl =		kamailio-perl-${VERSION}

CATEGORIES =		telephony
MAINTAINER =		port-pkg-maint@clark-communications.com

HOMEPAGE =		http://www.kamailio.org/

# GPL
PERMIT_PACKAGE_CDROM =	Yes
PERMIT_PACKAGE_FTP =	Yes
PERMIT_DISTFILES_CDROM =Yes
PERMIT_DISTFILES_FTP =	Yes

MASTER_SITES =		http://www.kamailio.org/pub/kamailio/${VERSION}/src/
DISTFILES =		${DISTNAME}-tls_src.tar.gz
WRKDIST =		${WRKDIR}/${DISTNAME}-tls

BUILD_DEPENDS =		::devel/gmake

WANTLIB-main =		c expat crypto ssl
LIB_DEPENDS-main =	xml2::textproc/libxml,-main,no_python
# kamctl and kamdbctl use bash
RUN_DEPENDS-main =	::shells/bash

WANTLIB-berkeleydb =	c
LIB_DEPENDS-berkeleydb =db::databases/db/v4,-main
RUN_DEPENDS-berkeleydb =::telephony/kamailio

LIB_DEPENDS-postgresql =pq:postgresql-client-*:databases/postgresql,-main
RUN_DEPENDS-postgresql =::telephony/kamailio

WANTLIB-mysql =		crypto m ssl z
LIB_DEPENDS-mysql =	mysqlclient:mysql-client-*:databases/mysql,-main
RUN_DEPENDS-mysql =	::telephony/kamailio

LIB_DEPENDS-radius =	radiusclient-ng:radiusclient-ng-*:net/radiusclient-ng
RUN_DEPENDS-radius =	::telephony/kamailio

LIB_DEPENDS-ldap =	ldap:openldap-client-*:databases/openldap,-main
RUN_DEPENDS-ldap =	::telephony/kamailio

LIB_DEPENDS-xmlrpc =	xmlrpc::net/xmlrpc-c \
			xmlrpc_server::net/xmlrpc-c \
			xmlrpc_util::net/xmlrpc-c \
			xmlrpc_xmlparse::net/xmlrpc-c \
			xmlrpc_xmltok::net/xmlrpc-c
RUN_DEPENDS-xmlrpc =	::telephony/kamailio

LIB_DEPENDS-xcap_client =	curl::net/curl \
				xml2::textproc/libxml,-main
RUN_DEPENDS-xcap_client =	::telephony/kamailio

LIB_DEPENDS-carrierroute =	confuse::devel/libconfuse
RUN_DEPENDS-carrierroute =	::telephony/kamailio

WANTLIB-snmpstats =	c m perl util
LIB_DEPENDS-snmpstats =	netsnmp:net-snmp-*:net/net-snmp \
			netsnmpagent:net-snmp-*:net/net-snmp \
			netsnmphelpers:net-snmp-*:net/net-snmp
RUN_DEPENDS-snmpstats =	::telephony/kamailio

RUN_DEPENDS-perl =	::telephony/kamailio \
			::databases/p5-ldap
WANTLIB-perl =		c m perl util

PSEUDO_FLAVORS =	no_berkeleydb no_postgresql no_mysql no_radius no_ldap no_xmlrpc no_xcap_client no_carrierroute no_snmpstats no_perl

KAMAILIO_MODULES =	cpl-c jabber presence presence_mwi presence_xml pua pua_bla pua_mi pua_usrloc pua_xmpp rls seas xmpp
FLAVOR ?=
MULTI_PACKAGES =	-main

.if !${FLAVOR:L:Mno_berkeleydb}
MULTI_PACKAGES +=	-berkeleydb
KAMAILIO_MODULES +=	db_berkeley
.endif
.if !${FLAVOR:L:Mno_postgresql}
MULTI_PACKAGES +=	-postgresql
KAMAILIO_MODULES +=	db_postgres
.endif
.if !${FLAVOR:L:Mno_mysql}
MULTI_PACKAGES +=	-mysql
KAMAILIO_MODULES +=	db_mysql
.endif
.if !${FLAVOR:L:Mno_radius}
MULTI_PACKAGES +=	-radius
KAMAILIO_MODULES +=	avp_radius auth_radius group_radius uri_radius peering
.endif
.if !${FLAVOR:L:Mno_ldap}
MULTI_PACKAGES +=	-ldap
KAMAILIO_MODULES +=	ldap h350
.endif
.if !${FLAVOR:L:Mno_xmlrpc}
MULTI_PACKAGES +=	-xmlrpc
KAMAILIO_MODULES +=	mi_xmlrpc
.endif
.if !${FLAVOR:L:Mno_xcap_client}
MULTI_PACKAGES +=	-xcap_client
KAMAILIO_MODULES +=	xcap_client
.endif
.if !${FLAVOR:L:Mno_snmpstats}
MULTI_PACKAGES +=	-snmpstats
KAMAILIO_MODULES +=	snmpstats
.endif
.if !${FLAVOR:L:Mno_carrierroute}
MULTI_PACKAGES +=	-carrierroute
KAMAILIO_MODULES +=	carrierroute
.endif
.if !${FLAVOR:L:Mno_perl}
MULTI_PACKAGES +=	-perl
KAMAILIO_MODULES +=	perl perlvdb
.endif

MAKE_FLAGS =		TLS=1 LIBDIR="lib" cfg-prefix="${WRKINST}" cfg-target="/etc/kamailio/" include_modules="${KAMAILIO_MODULES}"
FAKE_FLAGS =		PREFIX=${PREFIX}
USE_GMAKE =		Yes
CONFIGURE_STYLE =	none
DESTDIRNAME=BASEDIR
NO_REGRESS =		Yes

post-patch:
.for f in kamctl kamdbctl
	@cp ${WRKDIST}/scripts/${f} ${WRKDIST}/scripts/${f}.bak
	@sed -e '1s,^#!/bin/bash,#!/usr/local/bin/bash,' ${WRKDIST}/scripts/${f}.bak > ${WRKDIST}/scripts/${f}
.endfor

post-install:
	mv ${WRKINST}${SYSCONFDIR}/kamailio ${WRKINST}${LOCALBASE}/share/examples
.include <bsd.port.mk>
