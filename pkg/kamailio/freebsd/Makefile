# New ports collection makefile for:    ser
# Date created:        6 November 2002
# Whom:                andrei
#
# $FreeBSD$
#
# $Id$
#


PORTNAME= openser
PORTVERSION= 0.10.0
CATEGORIES= net
MASTER_SITES= http://openser.org/pub/openser/0.10.0/src/

MAINTAINER= daniel@voice-system.ro

.include <bsd.port.pre.mk>

.if ${OSVERSION} >= 501000
# FreeBSD 5.1 wants COMMENT and it doesn't like pkg-comment
COMMENT= Open SIP Express Router, very fast and configurable SIP proxy
COMMENTFILE= 
.endif

MAN8=openser.8
MAN5=openser.cfg.5
MANCOMPRESSED= no


USE_GMAKE= yes
# we override the DISTFILE NAME
DISTNAME= ${PORTNAME}-${PORTVERSION}_src
WRKSRC= ${WRKDIR}/${PORTNAME}-${PORTVERSION}

# build by default mysql and jabber
MAKE_ENV= include_modules="mysql jabber"




# depends on mysql and libexpat because of the above modules
BUILD_DEPENDS= ${LOCALBASE}/include/expat.h:${PORTSDIR}/textproc/expat* \
               ${LOCALBASE}/include/mysql/mysql.h:${PORTSDIR}/databases/mysql32*-client
LIB_DEPENDS=   expat:${PORTSDIR}/textproc/expat* \
               mysqlclient.10:${PORTSDIR}/databases/mysql32*-client

pre-fetch:


.include <bsd.port.post.mk>


