# New ports collection makefile for:    ser
# Date created:        6 November 2002
# Whom:                andrei
#
# $FreeBSD$
#
# $Id$
#


PORTNAME= ser
PORTVERSION= 0.8.10
CATEGORIES= net
MASTER_SITES= ftp://ftp.berlios.de/pub/ser/0.8.10/src/

MAINTAINER= pelinescu-onciul@fokus.gmd.de

MAN8=ser.8
MAN5=ser.cfg.5
MANCOMPRESSED= no


USE_GMAKE= yes
# we override the DISTFILE NAME
DISTNAME= ${PORTNAME}-${PORTVERSION}_src
WRKSRC= ${WRKDIR}/sip_router

pre-fetch:
	@echo "DEBUG: distfile=${DISTFILE}, distname=${DISTNAME}, w=${WRKSRC}"


.include <bsd.port.mk>


