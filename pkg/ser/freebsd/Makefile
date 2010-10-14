# New ports collection makefile for:    ser
# Date created:        6 November 2002
# Whom:                andrei
#
# $FreeBSD$
#
# $Id$
#


PORTNAME= ser
PORTVERSION= 0.9.4
CATEGORIES= net
MASTER_SITES= ftp://ftp.iptel.org/pub/ser/0.9.4/src/

MAINTAINER= andrei@iptel.org

.include <bsd.port.pre.mk>

.if ${OSVERSION} >= 501000
# FreeBSD 5.1 wants COMMENT and it doesn't like pkg-comment
COMMENT= Sip Express Router, very fast and configurable SIP proxy
COMMENTFILE= 
.endif

MAN8=ser.8
MAN5=ser.cfg.5
MANCOMPRESSED= no


USE_GMAKE= yes
# we override the DISTFILE NAME
DISTNAME= ${PORTNAME}-${PORTVERSION}_src
WRKSRC= ${WRKDIR}/${PORTNAME}-${PORTVERSION}

# build by default mysql and jabber
MAKE_ENV= include_modules="mysql jabber cpl-c pa"




# depends on mysql and libexpat because of the above modules
BUILD_DEPENDS= ${LOCALBASE}/include/expat.h:${PORTSDIR}/textproc/expat* \
               ${LOCALBASE}/include/mysql/mysql.h:${PORTSDIR}/databases/mysql*-client \
               ${LOCALBASE}/include/libxml2/libxml/uri.h:${PORTSDIR}/textproc/libxml2* 
LIB_DEPENDS=   expat:${PORTSDIR}/textproc/expat* \
               mysqlclient:${PORTSDIR}/databases/mysql*-client \
               xml2:${PORTSDIR}/textproc/libxml2

pre-fetch:


.include <bsd.port.post.mk>


