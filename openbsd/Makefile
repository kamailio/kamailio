# New ports collection makefile for:    ser
# Date created:        6 November 2002
# Whom:                andrei
#
# $OpenBSD$
#
# $Id$
#

COMMENT= "SIP Express Router"

PORTNAME= ser
PORTVERSION= 0.8.11pre29
CATEGORIES= net
MASTER_SITES= ftp://ftp.berlios.de/pub/ser/0.8.11pre29/src/

MAINTAINER= pelinescu-onciul@fokus.fraunhofer.de

# GPL
PERMIT_PACKAGE_CDROM=	Yes
PERMIT_PACKAGE_FTP=	Yes
PERMIT_DISTFILES_CDROM=	Yes
PERMIT_DISTFILES_FTP=	Yes


USE_GMAKE= yes
# we override the DISTFILE NAME
DISTNAME= ${PORTNAME}-${PORTVERSION}
DISTFILES= ${DISTNAME}_src.tar.gz
WRKSRC= ${WRKDIR}/${PORTNAME}-${PORTVERSION}

pre-fetch:
	@echo "DEBUG: distfile=${DISTFILE}, distname=${DISTNAME}, w=${WRKSRC}"


.include <bsd.port.mk>


