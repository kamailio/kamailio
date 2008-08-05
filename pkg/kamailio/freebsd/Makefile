# New ports collection makefile for:	openser
# Date created:				20 October 2005
# Whom:					jesusr
#
# $FreeBSD: ports/net/openser/Makefile,v 1.9 2007/08/20 09:12:42 miwi Exp $
#

PORTNAME=	openser
PORTVERSION=	1.3.0
CATEGORIES=	net
MASTER_SITES=	http://openser.org/pub/openser/${PORTVERSION}/src/
DISTNAME=	${PORTNAME}-${PORTVERSION}-tls_src

MAINTAINER=	jesusr@FreeBSD.org
COMMENT=	A very fast and configurable SIP proxy with TLS support

LIB_DEPENDS=	radiusclient-ng.2:${PORTSDIR}/net/radiusclient

WRKSRC=		${WRKDIR}/${PORTNAME}-${PORTVERSION}-tls

USE_GMAKE=	yes
MAKE_ENV=	CC="${CC}" \
		CC_EXTRA_OPTS="${CFLAGS}" \
		PTHREAD_CFLAGS="${PTHREAD_CFLAGS}" \
		PTHREAD_LIBS="${PTHREAD_LIBS}"

PLIST_FILES=	sbin/openser sbin/kamctl sbin/kamdbctl sbin/kamunix \
		lib/openser/kamctl/kamctl.base lib/openser/kamctl/kamctl.ctlbase \
		lib/openser/kamctl/kamctl.dbtext lib/openser/kamctl/kamctl.fifo \
		lib/openser/kamctl/kamctl.sqlbase lib/openser/kamctl/kamctl.unixsock \
		lib/openser/kamctl/kamdbctl.base lib/openser/kamctl/kamdbctl.dbtext \
		etc/openser/kamctlrc

PLIST_DIRS=	lib/openser/modules lib/openser/kamctl lib/openser

MODULES=	acc alias_db auth auth_db auth_diameter avpops benchmark \
		cfgutils dbtext dialog dispatcher diversion domain \
		domainpolicy enum exec flatstore gflags group imc lcr \
		mangler maxfwd mediaproxy mi_datagram mi_fifo msilo nathelper \
		options path pdt permissions pike registrar rr seas \
		siptrace sl sms speeddial sst statistics textops tm uac \
		uac_redirect uri uri_db usrloc xlog

RC_FILES=	acc/radiusclient.conf \
		acc/servers \
		dictionary.radius \
		openser.cfg
RC_DIRS=	acc

ONLY_FOR_ARCHS=	i386

MAN5=		openser.cfg.5
MAN8=		openser.8 \
		kamctl.8 \
		kamunix.8

PORTDOCS=	*

SUB_FILES=	pkg-install pkg-deinstall
SUB_LIST=	RC_DIRS="${RC_DIRS}" RC_FILES="${RC_FILES}"

OPTIONS=	MYSQL "MySQL support" on \
		POSTGRESQL "PostgreSQL support" off \
		UNIXODBC "UnixODBC support" off \
		CPL "CPL-C support" off \
		TLS "TLS support" off \
		SNMPSTATS "SNMPStats support" off

.include <bsd.port.pre.mk>

.if defined(WITH_MYSQL)
USE_MYSQL=	yes
MODULES+=	mysql
PLIST_FILES+=	lib/openser/kamctl/kamdbctl.mysql lib/openser/kamctl/kamctl.mysql
.else
MAKE_ENV+=	MYSQL=mysql
.endif

.if defined(WITH_POSTGRESQL)
USE_PGSQL=	yes
MODULES+=	postgres
PLIST_FILES+=	lib/openser/kamctl/kamdbctl.pgsql lib/openser/kamctl/kamctl.pgsql
.else
MAKE_ENV+=	POSTGRESQL=postgres
.endif

.if defined(WITH_UNIXODBC)
LIB_DEPENDS+=	odbc.1:${PORTSDIR}/databases/unixODBC
MODULES+=	unixodbc
.else
MAKE_ENV+=	UNIXODBC=unixodbc
.endif

.if defined(WITH_CPL)
USE_GNOME=	libxml2
MODULES+=	cpl-c
PLIST_FILES+=	etc/openser/cpl-06.dtd
.else
MAKE_ENV+=	CPL=cpl-c
.endif

.if defined(WITH_TLS)
MODULES+=	tlsops
.include "${PORTSDIR}/Mk/bsd.openssl.mk"
TLS_RC_FILES=	tls/rootCA/cacert.pem \
		tls/rootCA/certs/01.pem \
		tls/rootCA/index.txt \
		tls/rootCA/private/cakey.pem \
		tls/rootCA/serial \
		tls/user/user-calist.pem \
		tls/user/user-cert.pem \
		tls/user/user-cert_req.pem \
		tls/user/user-privkey.pem \
		tls/README \
		tls/ca.conf \
		tls/request.conf \
		tls/user.conf
RC_FILES+=	${TLS_RC_FILES}
TLS_RC_DIRS=	tls/user \
		tls/rootCA/certs \
		tls/rootCA/private \
		tls/rootCA \
		tls
RC_DIRS+=	${TLS_RC_DIRS}
.else
MAKE_ENV+=	TLSOPS=tlsops
MAKE_ARGS+=	TLS=""
.endif

.if defined(WITH_SNMPSTATS)
RUN_DEPENDS+=	snmpcheck:${PORTSDIR}/net-mgmt/net-snmp
MODULES+=	snmpstats
.else
MAKE_ENV+=	SNMPSTATS=snmpstats
.endif

PLIST_FILES+=	${MODULES:S|^|lib/openser/modules/|:S|$|.so|} \
		${RC_FILES:S|^|etc/openser/|:S|$|.default|}

post-patch:
.if defined(NOPORTDOCS)
	@${REINPLACE_CMD} -e 's|install-doc install-man|install-man|' \
		${WRKSRC}/Makefile
.endif
	@${REINPLACE_CMD} -e 's|-g -O9 ||' -e 's|-O9 ||' ${WRKSRC}/Makefile.defs
	@${REINPLACE_CMD} -e 's|/usr/local/etc/openser|${PREFIX}/etc/openser|' \
		${WRKSRC}/scripts/kamdbctl.dbtext
	@${REINPLACE_CMD} -e 's|/usr/local/share/openser|${PREFIX}/share/openser|' \
		${WRKSRC}/scripts/kamdbctl.dbtext
	@${REINPLACE_CMD} -e 's|/etc/openser|${PREFIX}/etc/openser|' \
		${WRKSRC}/modules/mediaproxy/README \
		${WRKSRC}/modules/cpl-c/README \
		${WRKSRC}/INSTALL
	@${REINPLACE_CMD} -e 's|/usr/local/sbin/openser|${PREFIX}/sbin/openser|' \
		${WRKSRC}/INSTALL
	@${REINPLACE_CMD} -e 's|/usr/local|${PREFIX}|' \
		${WRKSRC}/etc/openser.cfg \
		${WRKSRC}/scripts/kamctl \
		${WRKSRC}/scripts/kamctl.8 \
		${WRKSRC}/scripts/kamctl.base \
		${WRKSRC}/scripts/kamctl.db_berkeley \
		${WRKSRC}/scripts/kamctl.dbtext \
		${WRKSRC}/scripts/kamctlrc \
		${WRKSRC}/scripts/kamdbctl \
		${WRKSRC}/scripts/kamdbctl.base \
		${WRKSRC}/scripts/kamdbctl.db_berkeley \
		${WRKSRC}/scripts/kamdbctl.dbtext \
		${WRKSRC}/scripts/kamdbctl.mysql \
		${WRKSRC}/scripts/kamdbctl.pgsql \
		${WRKSRC}/modules/acc/README \
		${WRKSRC}/modules/avp_radius/README \
		${WRKSRC}/modules/db_berkeley/README \
		${WRKSRC}/modules/ldap/README \
		${WRKSRC}/modules/osp/README \
		${WRKSRC}/modules/perl/README \
		${WRKSRC}/modules/seas/README \
		${WRKSRC}/modules/snmpstats/README \
		${WRKSRC}/modules/speeddial/README \
		${WRKSRC}/modules/unixodbc/README
	@${REINPLACE_CMD} -e 's|/usr/local|${LOCALBASE}|' \
		${WRKSRC}/modules/acc/etc/radiusclient.conf
	@${REINPLACE_CMD} -e 's|/usr/local/etc/radiusclient|${PREFIX}/etc/openser/acc|' \
		${WRKSRC}/modules/acc/acc_mod.c ${WRKSRC}/modules/acc/README

post-install:
	${INSTALL_DATA} ${WRKSRC}/etc/openser.cfg \
		${PREFIX}/etc/openser/openser.cfg.default
	${MKDIR} ${PREFIX}/etc/openser/acc
	${INSTALL_DATA} ${WRKSRC}/modules/acc/etc/radiusclient.conf \
		${PREFIX}/etc/openser/acc/radiusclient.conf.default
	${INSTALL_DATA} ${WRKSRC}/modules/acc/etc/servers \
		${PREFIX}/etc/openser/acc/servers.default
.for d in ${TLS_RC_DIRS}
	${MKDIR} ${PREFIX}/etc/openser/${d}
.endfor
.for f in ${TLS_RC_FILES}
	${INSTALL_DATA} ${WRKSRC}/etc/${f} ${PREFIX}/etc/openser/${f}.default
.endfor
	@PKG_PREFIX=${PREFIX} ${SH} ${PKGINSTALL} ${PKGNAME} POST-INSTALL

.include <bsd.port.post.mk>
