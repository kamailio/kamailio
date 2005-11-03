# New ports collection makefile for:	openser
# Date created:				20 October 2005
# Whom:					jesusr
#
# $FreeBSD$
# $Id$
#

PORTNAME=	openser
PORTVERSION=	1.0.0
CATEGORIES=	net
MASTER_SITES=	http://openser.org/pub/openser/${PORTVERSION}/src/
DISTNAME=	${PORTNAME}-${PORTVERSION}-tls_src

MAINTAINER=	jesusr@FreeBSD.org
COMMENT=	A very fast and configurable SIP proxy with TLS support

LIB_DEPENDS=	radiusclient-ng.2:${PORTSDIR}/net/radiusclient

WRKSRC=		${WRKDIR}/${PORTNAME}-${PORTVERSION}-tls

USE_REINPLACE=	yes
USE_GMAKE=	yes
USE_GNOME=	libxml2
MAKE_ENV=	CC="${CC}" \
		PTHREAD_CFLAGS="${PTHREAD_CFLAGS}" \
		PTHREAD_LIBS="${PTHREAD_LIBS}"

PLIST_FILES=	sbin/openser sbin/openser_gen_ha1 sbin/openser_mysql.sh \
		sbin/openserctl sbin/openserunix
PLIST_DIRS=	lib/openser/modules lib/openser

MODULES=	acc alias_db auth auth_db auth_diameter avpops cpl-c dbtext \
		dispatcher diversion domain enum exec flatstore gflags group \
		lcr mangler maxfwd mediaproxy msilo nathelper options pa pdt \
		permissions pike registrar rr sl sms speeddial textops tm uac \
		uac_redirect uri uri_db usrloc xlog

RC_FILES=	acc/radiusclient.conf \
		acc/servers \
		acc/sip_dictionary \
		dictionary.radius \
		openser.cfg
RC_DIRS=	acc

MAN5=		openser.cfg.5
MAN8=		openser.8

PORTDOCS=	*

SUB_FILES=	pkg-install pkg-deinstall
SUB_LIST=	RC_DIRS="${RC_DIRS}" RC_FILES="${RC_FILES}"

OPTIONS=	MYSQL "MySQL support" on \
		POSTGRESQL "PostgreSQL support" on \
		TLS "TLS support" on

.include <bsd.port.pre.mk>

.if defined(WITH_MYSQL)
USE_MYSQL=	yes
MODULES+=	mysql
.else
MAKE_ENV+=	MYSQL=mysql
.endif

.if defined(WITH_POSTGRESQL)
USE_PGSQL=	yes
MODULES+=	postgres
.else
MAKE_ENV+=	POSTGRESQL=postgres
.endif

.if defined(WITH_TLS)
.include "${PORTSDIR}/Mk/bsd.openssl.mk"
PLIST_FILES+=	etc/openser/tls/README
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
	@${REINPLACE_CMD} -e 's|/usr/local/etc/ser|${PREFIX}/etc/openser|' \
		${WRKSRC}/scripts/sc.dbtext \
		${WRKSRC}/modules/dispatcher/README
	@${REINPLACE_CMD} -e 's|/etc/openser|${PREFIX}/etc/openser|' \
		${WRKSRC}/modules/mediaproxy/README
	@${REINPLACE_CMD} -e 's|/usr/local|${PREFIX}|' \
		${WRKSRC}/etc/openser.cfg \
		${WRKSRC}/scripts/mysqldb.sh \
		${WRKSRC}/scripts/postgresqldb.sh \
		${WRKSRC}/scripts/sc \
		${WRKSRC}/scripts/sc.dbtext \
		${WRKSRC}/modules/speeddial/README
	@${REINPLACE_CMD} -e 's|/home/jiri/sip_router/modules/acc/etc|${PREFIX}/etc/openser/acc|; \
		s|/usr/local|${LOCALBASE}|' \
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
	${INSTALL_DATA} ${WRKSRC}/modules/acc/etc/sip_dictionary \
		${PREFIX}/etc/openser/acc/sip_dictionary.default
.for d in ${TLS_RC_DIRS}
	${MKDIR} ${PREFIX}/etc/openser/${d}
.endfor
.for f in ${TLS_RC_FILES}
	${INSTALL_DATA} ${WRKSRC}/etc/${f} ${PREFIX}/etc/openser/${f}.default
.endfor
	@PKG_PREFIX=${PREFIX} ${SH} ${PKGINSTALL} ${PKGNAME} POST-INSTALL

.include <bsd.port.post.mk>
