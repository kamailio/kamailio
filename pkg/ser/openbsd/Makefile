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
PORTVERSION= 0.9.4
CATEGORIES= net
MASTER_SITES= ftp://ftp.iptel.org/pub/ser/0.9.4/src/

MAINTAINER= andrei@iptel.org 

MAN8=ser.8
MAN5=ser.cfg.5
# MANCOMPRESSED must not be defined in openbsd (defined==yes)

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

# build by default mysql and jabber
MAKE_ENV= include_modules="mysql jabber cpl-c pa"

# depends on mysql and libexpat because of the above modules
BUILD_DEPENDS= :expat-1.*:textproc/expat \
               :mysql-client-[3-].*:databases/mysql \
               :libxml-2.*:textproc/libxml
LIB_DEPENDS=   expat.2:expat-1.*:textproc/expat \
               lib/mysql/mysqlclient.10:mysql-client-[3-].*:databases/mysql \
               xml2:libxml-2.*:textproc/libxml



pre-fetch:


.include <bsd.port.mk>


