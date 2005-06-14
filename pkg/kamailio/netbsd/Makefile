# New ports collection makefile for:    openser
#
# $NetBSD$
#
# $Id$
#

COMMENT= "Open SIP Express Router"

PORTNAME= openser
PORTVERSION= 0.10.0
CATEGORIES= net
MASTER_SITES= http://openser.org/pub/openser/0.10.0/src/

MAINTAINER= daniel@voice-system.ro

MAN8=openser.8
MAN5=openser.cfg.5
# MANCOMPRESSED must not be specified (if defined is assumed to be yes)


USE_GMAKE= yes
# we override the DISTFILE NAME
DISTNAME= ${PORTNAME}-${PORTVERSION}
DISTFILES= ${DISTNAME}_src.tar.gz
WRKSRC= ${WRKDIR}/${PORTNAME}-${PORTVERSION}

# build by default mysql and jabber
MAKE_ENV= include_modules="mysql jabber"

# depends on expat and mysqlclient because of the above modules
DEPENDS= expat-[0-9]*:../../textproc/expat \
         mysql-client>=3.23.35:../../databases/mysql-client

#.include "/usr/pkgsrc/mk/bsd.prefs.mk"

pre-fetch:

.include "../../mk/bsd.pkg.mk"


