# Copyright 1999-2003 Fraunhofer Fokus
# Distributed under the terms of the GNU General Public License v2
# $Header$

DESCRIPTION="SIP Express Router"

HOMEPAGE="http://www.iptel.org/ser"
SRC_URI="ftp://ftp.berlios.de/pub/ser/0.8.11/src/${P}_src.tar.gz"

LICENSE="GPL-2"
SLOT="0"
KEYWORDS="~x86 ~ppc ~sparc"
IUSE="debug ipv6 mysql postgres"

DEPEND=">=sys-devel/gcc-2.95.3
		>=sys-devel/bison-1.35
		>=sys-devel/flex-2.5.4a
		mysql? ( >=dev-db/mysql-3.23.52 )
		postgres? ( >=dev-db/postgresql-7.3.4 )"

S="${WORKDIR}/${P}"

inc_mod=""
make_options=""

check_mods() {
	if [ "`use mysql`" ]; then
		inc_mod="${inc_mod} mysql"
	fi
	if [ "`use postgres`" ]; then
		inc_mod="${inc_mod} postgres"
	fi
	
	# test some additional modules for which
	# no USE variables exist
	
	# jabber module requires dev-libs/expat
	if [ -f "/usr/include/expat.h" ]; then
		inc_mod="${inc_mod} jabber"
	fi
	# Radius modules requires installed radiusclient
	# which is not in portage yet
	if [ -f "/usr/include/radiusclient.h" -o  -f "/usr/local/include/radisuclient.h" ]; then
		inc_mod="${inc_mod} auth_radius group_radius uri_radius"
	fi
}

src_compile() {
	if [ ! "`use ipv6`" ]; then
		cp Makefile.defs Makefile.defs.orig 
		sed -e "s/-DUSE_IPV6//g" Makefile.defs.orig > Makefile.defs;
	fi
	# optimization can result in strange debuging symbols so omit it in case
	if [ "`use debug`" ]; then
		make_options="${make_options} mode=debug"
	else
		make_options="${make_options} CFLAGS=${CFLAGS}"
	fi

	check_mods

	make all "${make_options}" \
		prefix=${D}/ \
		include_modules="${inc_mod}" \
		cfg-prefix=/ \
		cfg-target=/etc/ser/ || die
}

src_install () {
	check_mods

	make install \
		prefix=${D}/ \
		include_modules="${inc_mod}" \
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

	# fix what the Makefile don't do
	if [ ! "`use mysql`" ]; then
		rm ${D}/usr/sbin/ser_mysql.sh
	fi
}

pkg_postinst() {
	einfo "WARNING: If you upgraded from a previous Ser version"
	einfo "please read the README, NEWS and INSTALL files in the"
	einfo "documentation directory because the database and the"
	einfo "configuration file of old Ser versions are incompatible"
	einfo "with the current version."
}

pkg_prerm () {
	/etc/init.d/ser stop >/dev/null
}
