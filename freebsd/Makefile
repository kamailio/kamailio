# New ports collection makefile for:    ser
# Date created:        6 November 2002
# Whom:                andrei
#
# $FreeBSD$
#
# $Id$
#


PORTNAME= ser
PORTVERSION= 0.8.11
CATEGORIES= net
MASTER_SITES= ftp://ftp.berlios.de/pub/ser/0.8.11/src/

MAINTAINER= pelinescu-onciul@fokus.fraunhofer.de

MAN8=ser.8
MAN5=ser.cfg.5
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


.include <bsd.port.mk>


