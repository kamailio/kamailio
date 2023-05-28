# Copyright 1999-2020 Gentoo Foundation
# Distributed under the terms of the GNU General Public License v2

EAPI=6

inherit eutils flag-o-matic toolchain-funcs user

IUSE="ipv6 mysql radius postgres jabber ssl odbc sctp rtpengine redis pua ims presence sqlite snmp json http ldap geoip2 erlang xml jansson lua evapi"

DESCRIPTION="The Open Source SIP Server"
HOMEPAGE="https://www.kamailio.org/"

if [[ ${PV} == 9999 ]]; then
	inherit git-r3
	EGIT_REPO_URI="https://github.com/kamailio/kamailio.git"
else
	SRC_URI="http://www.${PN}.org/pub/${PN}/${PV}/src/${P}_src.tar.gz"
fi

SLOT="0"
LICENSE="GPL-2+"
KEYWORDS="~amd64 ~x86"

RDEPEND=">=sys-devel/bison-1.35
	>=sys-devel/flex-2.5.4a
	app-text/dos2unix
	dev-libs/libpcre
	>=dev-libs/libical-3.0.5
	ssl? ( dev-libs/openssl )
	mysql? ( virtual/mysql )
	radius? ( >=net-dialup/radiusclient-ng-0.5.0 )
	postgres? ( dev-db/libpq )
	jabber? ( dev-libs/expat )
	odbc? ( dev-db/unixODBC )
	sctp? ( net-misc/lksctp-tools )
	redis? ( dev-db/redis )
	rtpengine? ( net-misc/ngcp-rtpengine )
	ldap? ( net-nds/openldap )
	sqlite? ( dev-db/sqlite )
	snmp? ( net-analyzer/net-snmp )
	xml? ( dev-libs/libxml2 )
	jansson? ( dev-libs/jansson )
	json? ( dev-libs/json-c )
	lua? ( dev-lang/lua )
	geoip2? ( dev-libs/libmaxminddb )
	evapi? ( >=dev-libs/libev-4.23 )"

DEPEND="${RDEPEND}"

src_prepare() {
	eapply_user
}

src_configure() {
	use ipv6 || \
		sed -i -e "s/-DUSE_IPV6//g" Makefile.defs

	use ssl && \
		sed -i -e "s:^#\(TLS=1\).*:\1:" Makefile && \
		KAMODULES="${KAMODULES} tls"

	use mysql && KAMODULES="${KAMODULES} db_mysql"

	use radius && KAMODULES="${KAMODULES} acc_radius auth_radius misc_radius"

	use jabber && KAMODULES="${KAMODULES} xmpp"
	use jabber && \
		use pua && \
		KAMODULES="${KAMODULES} pua_xmpp"

	use postgres && KAMODULES="${KAMODULES} db_postgres"

	use odbc && KAMODULES="${KAMODULES} db_unixodbc"

	use sqlite && KAMODULES="${KAMODULES} db_sqlite"

	use sctp && KAMODULES="${KAMODULES} sctp"

	use redis && KAMODULES="${KAMODULES} db_redis ndb_redis topos_redis"

	use json && KAMODULES="${KAMODULES} json acc_json jsonrpcc"
	use json && \
		use pua && \
		KAMODULES="${KAMODULES} pua_json"

	use pua && KAMODULES="${KAMODULES} pua pua_bla pua_dialoginfo pua_reginfo pua_rpc pua_usrloc"

	use ims && KAMODULES="${KAMODULES} ims_auth ims_charging ims_dialog ims_diameter_server ims_icscf ims_ipsec_pcscf ims_isc ims_ocs ims_qos ims_registrar_pcscf ims_registrar_scscf ims_usrloc_pcscf ims_usrloc_scscf cdp cdp_avp"

	use presence && KAMODULES="${KAMODULES} presence presence_conference presence_dialoginfo presence_mwi presence_profile presence_reginfo presence_xml"

	use snmp && KAMODULES="${KAMODULES} snmpstats"

	use http && KAMODULES="${KAMODULES} http_async_client http_client"

	use ldap && KAMODULES="${KAMODULES} ldap"

	use geoip2 && KAMODULES="${KAMODULES} geoip2"

	use erlang && KAMODULES="${KAMODULES} erlang"

	use evapi && KAMODULES="${KAMODULES} evapi"

	use xml && KAMODULES="${KAMODULES} xmlops"

	use lua && KAMODULES="${KAMODULES} app_lua"

	use jansson && KAMODULES="${KAMODULES} jansson"

	KAMODULES="${KAMODULES} dialplan lcr outbound utils regex uuid"
}

src_compile() {
	use amd64 && append-flags "-fPIC"
	use json && append-flags "-I/usr/include/json-c"

	emake \
		CC="$(tc-getCC)" \
		CPU_TYPE="$(get-flag march)" \
		mode="release" \
		prefix="/usr" \
		include_modules="${KAMODULES}" \
		cfg_prefix="/etc" \
		cfg_dir="${PN}/" \
		cfg_target="/etc/${PN}/" \
		doc_dir="share/doc/${PF}/" \
		all || die
}

src_install () {
	emake \
		BASEDIR="${D}" \
		mode="release" \
		prefix="/usr" \
		include_modules="${KAMODULES}" \
		cfg_prefix="${D}/etc" \
		cfg_dir="${PN}/" \
		cfg_target="/etc/${PN}/" \
		doc_dir="share/doc/${PF}/" \
		share_prefix="${D}/usr/" \
		share_dir="share/${PF}/" \
		data_prefix="${D}/usr" \
		data_dir="share/${PF}/" \
		data_target="/usr/share/${PF}/" \
		install || die

	newinitd "${FILESDIR}/${PN}".initd ${PN}
	newconfd "${FILESDIR}/${PN}".confd ${PN}

	cd "${S}"
	dodoc ChangeLog CODE_OF_CONDUCT.md COPYING INSTALL ISSUES README README.md
}

pkg_preinst() {
	if [[ -z "$(egetent passwd ${PN})" ]]; then
		einfo "Adding ${PN} user and group"
		enewgroup ${PN}
		enewuser  ${PN} -1 -1 /dev/null ${PN}
	fi
	chown -R root:${PN}  ${D}/etc/${PN}
	chmod -R u=rwX,g=rX,o= ${D}/etc/${PN}
}