# Copyright 1999-2002 Gentoo Technologies, Inc.
# Distributed under the terms of the GNU General Public License v2
# $Header$

DESCRIPTION="SIP Express Router"

HOMEPAGE="http://www.iptel.org/ser"
SRC_URI="http://download.berlios.de/${P}.tar.gz"

LICENSE="GPL-2"
SLOT="0"
KEYWORDS="x86"

DEPEND=">=sys-devel/gcc-2.95.3
		>=sys-devel/bison-1.35
		>=sys-devel/flex-2.5.4a"

S="${WORKDIR}/sip_router"

src_compile() {
	if [ ! "`use ipv6`" ]; then
		cp Makefile.defs Makefile.defs.orig 
		sed -e "s/-DUSE_IPV6//g" Makefile.defs.orig > Makefile.defs;
	fi
	make CFLAGS="${CFLAGS}" all || die
	cd utils/gen_ha1
	make || die
}

src_install () {
	make install \
		bin-prefix=${D}/usr/sbin \
		bin-dir="" \
		cfg-prefix=${D}/etc \
		cfg-dir=ser/ \
		modules-prefix=${D}/usr/lib/ser \
		modules-dir=modules \
		man-prefix=${D}/usr/share/man \
		man-dir="" \
		doc-prefix=${D}/usr/share/doc \
		doc-dir=${P} || die
	exeinto /etc/init.d
	newexe gentoo/ser.init ser
	exeinto /usr/bin
	newexe utils/gen_ha1/gen_ha1 gen_ha1
	exeinto /usr/sbin
	newexe scripts/harv_ser.sh harv_ser.sh
	newexe scripts/sc serctl
	newexe scripts/ser_mysql.sh ser_mysql.sh
}
