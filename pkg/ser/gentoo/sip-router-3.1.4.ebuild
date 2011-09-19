# Copyright 1999-2011 Gentoo Foundation
# Distributed under the terms of the GNU General Public License v2
# $Header: $

inherit eutils flag-o-matic toolchain-funcs multilib

DESCRIPTION="Sip-Router (Kamailio/SER) is an Open Source SIP Server"
HOMEPAGE="http://sip-router.org/"
MY_P="${P/sip-router/kamailio}"
SRC_URI="http://www.kamailio.org/pub/kamailio/${PV}/src/${MY_P}_src.tar.gz"
S=${WORKDIR}/${MY_P}

SLOT="0"
LICENSE="GPL-2"
KEYWORDS="~amd64 ~x86"

#Documentation can be found here: http://www.kamailio.org/docs/modules/3.1.x/
IUSE="flavour_kamailio flavour_ser debug ipv6 sctp
group_standard group_standard_dep group_mysql group_radius group_postgres group_presence group_stable group_experimental
group_kstandard group_kmysql group_kradius group_kpostgres group_kpresence group_kxml group_kperl group_kldap
acc acc_radius alias_db app_lua app_python auth auth_identity auth_db auth_diameter auth_radius avpops
benchmark blst
call_control carrierroute cfg_db cfg_rpc cfgutils counters cpl-c ctl
db_berkeley db_flatstore db_mysql db_oracle db_postgres db_text db_unixodbc
debugger dialog dialplan dispatcher diversion domain domainpolicy drouting
enum exec
geoip group
h350 htable imc iptrtpproxy jabber kex
lcr ldap
matrix maxfwd mediaproxy memcached misc_radius mi_datagram mi_fifo mi_rpc mi_xmlrpc mqueue msilo mtree
nathelper nat_traversal
osp
path pdb pdt peering perl perlvdb permissions pike pipelimit prefix_route
presence presence_conference presence_dialoginfo presence_mwi presence_xml
pua pua_bla pua_dialoginfo pua_mi pua_usrloc pua_xmpp purple pv
qos
ratelimit regex registrar rls rtimer rr rtpproxy
sanity seas siptrace siputils sl sms snmpstats speeddial sqlops statistics sst
textops textopsx tls tm tmx topoh
uac uac_redirect uri_db userblacklist usrloc utils
xcap_client xcap_server xhttp xlog xmlops xmlrpc xmpp"

#osp? ( net-libs/osptoolkit )
#pdb? ( pdb-server )
#seas? ( www.wesip.eu )

RDEPEND="
	>=sys-libs/ncurses-5.7
	>=sys-libs/readline-6.1_p2
	group_mysql? ( >=dev-db/mysql-5.1.50 sys-libs/zlib )
	group_radius? ( >=net-dialup/radiusclient-ng-0.5.0 )
	group_presence? ( dev-libs/libxml2 net-misc/curl )
	group_postgres? ( dev-db/postgresql-base )
	group_standard? ( dev-libs/libxml2 dev-libs/openssl net-misc/curl )
	group_kmysql? ( >=dev-db/mysql-5.1.50 sys-libs/zlib )
	group_kradius? ( >=net-dialup/radiusclient-ng-0.5.0 )
	group_kpresence? ( dev-libs/libxml2 net-misc/curl )
	group_kpostgres? ( dev-db/postgresql-base )
	group_kstandard? ( dev-libs/libxml2 dev-libs/openssl net-misc/curl )
	group_kxml? ( dev-libs/libxml2 dev-libs/xmlrpc-c )
	group_kperl? ( dev-lang/perl dev-perl/perl-ldap )
	group_kldap? ( net-nds/openldap )
	acc_radius? ( net-dialup/radiusclient-ng )
	app_lua? ( dev-lang/lua )
	app_python? ( dev-lang/python )
	auth_identity? ( dev-libs/openssl net-misc/curl )
	carrierroute? ( dev-libs/confuse )
	cpl-c? ( dev-libs/libxml2 )
	db_berkeley? ( >=sys-libs/db-4.6 )
	db_mysql? ( >=dev-db/mysql-5.1.50 )
	db_oracle? ( dev-db/oracle-instantclient-basic )
	db_postgres? ( dev-db/postgresql-base )
        db_unixodbc? ( dev-db/unixODBC )
	dialplan? ( dev-libs/libpcre )
	geoip? ( dev-libs/geoip )
	h350? ( net-nds/openldap )
	jabber? ( dev-libs/expat )
	lcr? ( dev-libs/libpcre )
	ldap? ( net-nds/openldap )
	memcached? ( dev-libs/libmemcache net-misc/memcached )
	mi_xmlrpc? ( dev-libs/libxml2 dev-libs/xmlrpc-c )
	peering? ( net-dialup/radiusclient-ng )
	perl? ( dev-lang/perl dev-perl/perl-ldap )
	presence? ( dev-libs/libxml2 )
	presence_conference? ( dev-libs/libxml2 )
	presence_xml? ( dev-libs/libxml2 )
	pua? ( dev-libs/libxml2 )
	pua_bla? ( dev-libs/libxml2 )
	pua_dialoginfo? ( dev-libs/libxml2 )
	pua_usrloc? ( dev-libs/libxml2 )
	pua_xmpp? ( dev-libs/libxml2 )
	purple? ( net-im/pidgin )
	regex? ( dev-libs/libpcre )
	rls? ( dev-libs/libxml2 )
	snmpstats? ( net-analyzer/net-snmp sys-apps/lm_sensors )
	tls? (
		sys-libs/zlib
		>=dev-libs/openssl-1.0.0a-r1
	)
	utils? ( net-misc/curl )
	xcap_client? ( dev-libs/libxml2 net-misc/curl )
	xcap_server? ( dev-libs/libxml2 )
	xmlops? ( dev-libs/libxml2 )
	xmpp? ( dev-libs/expat )
"
DEPEND="${RDEPEND}
	>=sys-devel/bison-1.35
	>=sys-devel/flex-2.5.4a
	app-text/docbook2X"

src_unpack() {
	unpack ${A}
	cd "${S}"

	use ipv6 || \
		sed -i -e "s/-DUSE_IPV6//g" Makefile.defs || die
}

src_compile() {
	# iptrtpproxy broken as the needed netfilter module is not supported
	local mod_exc="iptrtpproxy"
	local group_inc=""
	local k=""
	if use flavour_kamailio; then
		k="k"
		use group_kxml && group_inc="${group_inc} kxml"
		use group_kperl && group_inc="${group_inc} kperl"
		use group_kldap && group_inc="${group_inc} kldap"
	fi
	# you can USE flavour=kamailio but also group_standard. It will be converted to group_kstandard
	# same as mysql/kmysql, postgres/kpostgres, radius/kradius, presence/kpresence
	(use group_standard || use group_kstandard) && group_inc="${group_inc} ${k}standard"
	use group_standard_dep && group_inc="${group_inc} standard_dep"
	(use group_mysql || use group_kmysql) && group_inc="${group_inc} ${k}mysql"
	(use group_radius || use group_kradius) && group_inc="${group_inc} ${k}radius"
	(use group_postgres || use group_kpostgres) && group_inc="${group_inc} ${k}postgres"
	(use group_presence || use group_kpresence) && group_inc="${group_inc} ${k}presence"
	use group_stable && group_inc="${group_inc} stable"
	use group_experimental && group_inc="${group_inc} experimental"
	# TODO: skip_modules?

	local mod_inc=""
	# some IUSE flags must not be included here in mod_inc
	# e.g.: flavour_kamailio, flavour_ser, debug, sctp, ipv6
	for i in ${IUSE[@]}; do
		for j in ${i[@]}; do
			[[ ! "${i}" =~ "flavour_" ]] && \
				[ ! "${i}" == "debug" ] && \
				[ ! "${i}" == "ipv6" ] && \
				[ ! "${i}" == "sctp" ] && \
				[[ ! "${i}" =~ "group_" ]] && \
			use "${i}" && mod_inc="${mod_inc} ${i}"
		done
	done

	if use tls; then
		tls_hooks=1
	else
		tls_hooks=0
	fi

	if use debug; then
		mode=debug
	else
		mode=release
	fi

	if use flavour_kamailio; then
		flavour=kamailio
	else
		flavour=ser # defaults to SER compatibility names
	fi

	if use sctp; then
		sctp=1
	else
		sctp=0
	fi

	emake \
		CC="$(tc-getCC)" \
		CPU_TYPE="$(get-flag march)" \
		SCTP="${sctp}" \
		CC_EXTRA_OPTS=-I/usr/gnu/include \
		mode="${mode}" \
		TLS_HOOKS="${tls_hooks}" \
		FLAVOUR="${flavour}" \
		group_include="${group_inc}" \
		include_modules="${mod_inc}" \
		exclude_modules="${mod_exc}" \
		prefix="/" \
		all || die "emake all failed"
}

src_install() {
	emake -j1 \
		BASEDIR="${D}" \
		FLAVOUR="${flavour}" \
		prefix="/" \
		bin_dir=/usr/sbin/ \
		cfg_dir=/etc/${flavour}/ \
		lib_dir=/usr/$(get_libdir)/${flavour}/ \
		modules_dir="/usr/$(get_libdir)/${flavour}/" \
		man_dir="/usr/share/man/" \
		doc_dir="/usr/share/doc/${flavour}/" \
		install || die "emake install failed"

	sed -e "s/sip-router/${flavour}/g" \
		${FILESDIR}/ser.initd > ${flavour}.initd || die
	sed -e "s/sip-router/${flavour}/g" \
		${FILESDIR}/ser.confd > ${flavour}.confd || die

	newinitd "${flavour}".initd "${flavour}"
	newconfd "${flavour}".confd "${flavour}"
}

pkg_preinst() {
	if [[ -z "$(egetent passwd ${flavour})" ]]; then
		einfo "Adding ${flavour} user and group"
		enewgroup "${flavour}"
		enewuser  "${flavour}" -1 -1 /dev/null "${flavour}"
	fi

	chown -R root:"${flavour}"  "${D}/etc/${flavour}"
	chmod -R u=rwX,g=rX,o= "${D}/etc/${flavour}"

	has_version <="${CATEGORY}/ser-0.9.8"
	previous_installed_version=$?
	if [[ $previous_installed_version = 1 ]] ; then
		elog "You have a previous version of SER on ${ROOT}etc/ser"
		elog "Consider or verify to remove it (emerge -C ser)."
		elog
		elog "Sip-Router may not could be installed/merged. See your elog."
	fi
}

pkg_postinst() {
	if [ use mediaproxy ]; then
		einfo "You have enabled mediaproxy support. In order to use it, you have
		to run it somewhere."
	fi
	if [ use rtpproxy ]; then
		einfo "You have enabled rtpproxy support. In order to use it, you have
		to run it somewhere."
	fi
}

pkg_prerm () {
	/etc/init.d/"${flavour}" stop >/dev/null
}
