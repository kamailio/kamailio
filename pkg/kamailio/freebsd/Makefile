# New ports collection makefile for:	openser
# Date created:				20 October 2005
# Whom:					jesusr
#
# $FreeBSD: ports/net/openser/Makefile,v 1.4 2006/06/22 14:08:48 johans Exp $
#

PORTNAME=	openser
PORTVERSION=	1.1.1
CATEGORIES=	net
MASTER_SITES=	http://openser.org/pub/openser/${PORTVERSION}/src/
DISTNAME=	${PORTNAME}-${PORTVERSION}-tls_src

MAINTAINER=	jesusr@FreeBSD.org
COMMENT=	A very fast and configurable SIP proxy with TLS support

LIB_DEPENDS=	radiusclient-ng.2:${PORTSDIR}/net/radiusclient

WRKSRC=		${WRKDIR}/${PORTNAME}-${PORTVERSION}-tls

USE_GMAKE=	yes
USE_GNOME=	libxml2
MAKE_ENV=	CC="${CC}" \
		CC_EXTRA_OPTS="${CFLAGS}" \
		PTHREAD_CFLAGS="${PTHREAD_CFLAGS}" \
		PTHREAD_LIBS="${PTHREAD_LIBS}"

PLIST_FILES=	sbin/openser sbin/openserctl sbin/openserunix \
		lib/openser/openserctl/openserctl.base lib/openser/openserctl/openserctl.ctlbase \
		lib/openser/openserctl/openserctl.fifo lib/openser/openserctl/openserctl.sqlbase \
		lib/openser/openserctl/openserctl.unixsock

PLIST_DIRS=	lib/openser/modules lib/openser/openserctl lib/openser

MODULES=	acc alias_db auth auth_db auth_diameter avpops cpl-c dbtext \
		dialog dispatcher diversion domain enum exec flatstore gflags \
		group lcr mangler maxfwd mediaproxy msilo nathelper options \
		pa path pdt permissions pike registrar rr siptrace sl sms \
		speeddial statistics textops tm uac uac_redirect uri \
		uri_db usrloc xlog

RC_FILES=	acc/radiusclient.conf \
		acc/servers \
		dictionary.radius \
		openser.cfg
RC_DIRS=	acc

MAN5=		openser.cfg.5
MAN8=		openser.8 \
		openserctl.8

PORTDOCS=	*

SUB_FILES=	pkg-install pkg-deinstall
SUB_LIST=	RC_DIRS="${RC_DIRS}" RC_FILES="${RC_FILES}"

OPTIONS=	MYSQL "MySQL support" on \
		POSTGRESQL "PostgreSQL support" on \
		UNIXODBC "UnixODBC support" on \
		TLS "TLS support" on

.include <bsd.port.pre.mk>

.if defined(WITH_MYSQL)
USE_MYSQL=	yes
MODULES+=	mysql
PLIST_FILES+=	lib/openser/openserctl/openserctl.mysql sbin/openser_mysql.sh
.else
MAKE_ENV+=	MYSQL=mysql
.endif

.if defined(WITH_POSTGRESQL)
USE_PGSQL=	yes
MODULES+=	postgres
PLIST_FILES+=	lib/openser/openserctl/openserctl.pgsql sbin/openser_postgresql.sh
.else
MAKE_ENV+=	POSTGRESQL=postgres
.endif

.if defined(WITH_UNIXODBC)
LIB_DEPENDS+=	odbc.1:${PORTSDIR}/databases/unixODBC
MODULES+=	unixodbc
.else
MAKE_ENV+=	UNIXODBC=unixodbc
.endif

.if defined(WITH_TLS)
.include "${PORTSDIR}/Mk/bsd.openssl.mk"
TLS_RC_FILES=	tls/rootCA/cacert.pem \
		tls/rootCA/certs/01.pem \
		tls/rootCA/index.txt \
		tls/rootCA/private/cakey.pem \
		tls/rootCA/serial \
		tls/user/user-calist.pem \
		tls/user/user-cert.pem \
		tls/user/user-cert_req.pem \
		tls/user/user-privkey.pem
RC_FILES+=	${TLS_RC_FILES}
TLS_RC_DIRS=	tls/user \
		tls/rootCA/certs \
		tls/rootCA/private \
		tls/rootCA \
		tls
RC_DIRS+=	${TLS_RC_DIRS}
.else
MAKE_ARGS+=	TLS=""
.endif

PLIST_FILES+=	${MODULES:S|^|lib/openser/modules/|:S|$|.so|} \
		${RC_FILES:S|^|etc/openser/|:S|$|.default|}

post-patch:
.if defined(NOPORTDOCS)
	@${REINPLACE_CMD} -e 's|install-doc install-man|install-man|' \
		${WRKSRC}/Makefile
.endif
	@${REINPLACE_CMD} -e 's|-g -O9 ||' -e 's|-O9 ||' ${WRKSRC}/Makefile.defs
	@${REINPLACE_CMD} -e 's|/usr/local/etc/ser|${PREFIX}/etc/openser|' \
		${WRKSRC}/scripts/sc.dbtext \
		${WRKSRC}/modules/dispatcher/README
	@${REINPLACE_CMD} -e 's|/etc/openser|${PREFIX}/etc/openser|' \
		${WRKSRC}/modules/mediaproxy/README \
		${WRKSRC}/modules/cpl-c/README \
		${WRKSRC}/INSTALL
	@${REINPLACE_CMD} -e 's|/usr/local|${PREFIX}|' \
		${WRKSRC}/etc/openser.cfg \
		${WRKSRC}/scripts/mysqldb.sh \
		${WRKSRC}/scripts/postgresqldb.sh \
		${WRKSRC}/scripts/sc.dbtext \
		${WRKSRC}/modules/speeddial/README
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
