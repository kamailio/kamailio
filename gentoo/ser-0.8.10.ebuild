# Copyright 1999-2002 Gentoo Technologies, Inc.
# Distributed under the terms of the GNU General Public License v2
# $Id$

DESCRIPTION="SIP Express Router"

HOMEPAGE="http://www.iptel.org/ser"
SRC_URI="ftp://ftp.berlios.de/pub/ser/0.8.10/src/${P}_src.tar.gz"

LICENSE="GPL-2"
SLOT="0"
KEYWORDS="~x86"

DEPEND=">=sys-devel/gcc-2.95.3
		>=sys-devel/bison-1.35
		>=sys-devel/flex-2.5.4a
		mysql? ( >=dev-db/mysql-3.23.52 )
		dev-libs/expat"

S="${WORKDIR}/${P}"

src_compile() {
	if [ ! "`use ipv6`" ]; then
		cp Makefile.defs Makefile.defs.orig 
		sed -e "s/-DUSE_IPV6//g" Makefile.defs.orig > Makefile.defs;
	fi
	local exclude="CVS radius_acc radius_auth snmp"
	use mysql || exclude="${exclude} mysql"
	make all CFLAGS="${CFLAGS}" \
		prefix=${D}/ \
		exclude_modules="${exclude}" \
		cfg-prefix=/ \
		cfg-target=/etc/ser/ || die
}

src_install () {
	local exclude="CVS radius_acc radius_auth snmp"
	use mysql || exclude="${exclude} mysql"
	make install \
		prefix=${D}/ \
		exclude_modules="${exclude}" \
		bin-prefix=${D}/usr/sbin \
		bin-dir="" \
		cfg-prefix=${D}/etc \
		cfg-dir=ser/ \
		cfg-target=/etc/ser \
		modules-prefix=${D}/usr/lib/ser \
		modules-dir=modules \
		modules-target=/usr/lib/ser/modules/ \
		man-prefix=${D}/usr/share/man \
		man-dir="" \
		doc-prefix=${D}/usr/share/doc \
		doc-dir=${P} || die
	exeinto /etc/init.d
	newexe gentoo/ser.init ser
	exeinto /usr/sbin
	newexe scripts/harv_ser.sh harv_ser.sh
#	exeinto /usr/bin
#	newexe utils/gen_ha1/gen_ha1 gen_ha1
}

pkg_prerm () {
	/etc/init.d/ser stop >/dev/null
}
