# Kamailio build system Module groups definitions
#

# lists are sets of modules that don't include modules that are in other lists -
# break the lists as needed in order to use them to build desired groups and
# packages
#

# * basic used modules, with no extra dependency (widespread usage)
set(MOD_LIST_BASIC
    async
    auth
    benchmark
    blst
    cfg_rpc
    cfgutils
    corex
    counters
    ctl
    debugger
    diversion
    enum
    exec
    ipops
    kex
    mangler
    maxfwd
    mediaproxy
    mqueue
    nat_traversal
    nathelper
    path
    pike
    pv
    ratelimit
    rr
    rtimer
    rtpproxy
    sanity
    sdpops
    siputils
    sl
    statistics
    textops
    textopsx
    tm
    tmx
    topoh
    xlog
    rtpengine
    stun
    sipt
    tcpops
    auth_xkeys
    smsops
    tsilo
    cfgt
    statsc
    topos
    sipdump
    pv_headers
    kemix
)

# * extra used modules, with no extra dependency
set(MOD_LIST_EXTRA
    avp
    auth_diameter
    call_control
    call_obj
    dmq
    domainpolicy
    msrp
    carrierroute
    pdb
    qos
    sca
    seas
    sms
    sst
    timer
    tmrec
    uac_redirect
    xhttp
    xhttp_rpc
    xprint
    jsonrpcs
    nosip
    dmq_usrloc
    statsd
    rtjson
    log_custom
    keepalive
    ss7ops
    acc_diameter
    evrexec
    file_out
    sipjson
    lrkproxy
    math
    posops
    xhttp_prom
    dlgs
    sworker
    influxdbc
)

# * common modules depending on database
set(MOD_LIST_DB
    acc
    alias_db
    auth_db
    avpops
    cfg_db
    db_text
    db_flatstore
    db_cluster
    dialog
    dispatcher
    domain
    drouting
    group
    htable
    imc
    matrix
    mohqueue
    msilo
    mtree
    p_usrloc
    pdt
    permissions
    pipelimit
    prefix_route
    registrar
    sipcapture
    siptrace
    speeddial
    sqlops
    uac
    uri_db
    userblocklist
    usrloc
    secfilter
)

# * common modules depending on database, using UID db schema
set(MOD_LIST_DBUID db2_ops uid_auth_db uid_avp_db uid_domain uid_gflags
                   uid_uri_db
)

# * modules for devel purposes
set(MOD_LIST_DEVEL misctest print print_lib)

# * modules depending on pcre3 library
set(MOD_LIST_PCRE dialplan lcr regex)

# * modules depending on radius client library
set(MOD_LIST_RADIUS acc_radius auth_radius misc_radius peering)

# * modules depending on ldap client library
set(MOD_LIST_LDAP db2_ldap h350 ldap)

# * modules depending on mysql client library
set(MOD_LIST_MYSQL db_mysql)

# * modules depending on postgres client library
set(MOD_LIST_POSTGRES db_postgres)

# * modules depending on unixodbc library
set(MOD_LIST_UNIXODBC db_unixodbc)

# * modules depending on mysql cassandra library
set(MOD_LIST_CASSANDRA db_cassandra ndb_cassandra)

# * modules depending on xml2 library
set(MOD_LIST_CPLC cplc)

# * modules depending on xml2 library
set(MOD_LIST_XMLDEPS xhttp_pi xmlrpc xmlops)

# * modules depending on net-snmp library
set(MOD_LIST_SNMPSTATS snmpstats)

# * modules depending on expat library
set(MOD_LIST_XMPP xmpp)

# * modules depending on bdb (db4) library
set(MOD_LIST_BERKELEY db_berkeley)

# * modules depending on curl library
set(MOD_LIST_UTILS utils http_client lost slack)

# * modules depending on curl and libevent2 library
set(MOD_LIST_HTTP_ASYNC http_async_client)

# * modules depending on memcache library
set(MOD_LIST_MEMCACHED memcached)

# * modules depending on openssl library
set(MOD_LIST_TLSDEPS crypto tls)

# * modules depending on static openssl library
set(MOD_LIST_TLSA tlsa)

# * modules depending on static wolfssl library
set(MOD_LIST_TLS_WOLFSSL tls_wolfssl)

# * modules depending on openssl library
set(MOD_LIST_OUTBOUND outbound)

# * modules depending on openssl and unistring library
set(MOD_LIST_WEBSOCKET websocket)

# * modules depending on libval-threads libcrypto libsres libpthread
set(MOD_LIST_DNSSEC dnssec)

# * modules depending on libsctp
set(MOD_LIST_SCTP sctp)

# * modules depending on openssl library
set(MOD_LIST_AUTHEPH auth_ephemeral)

# * modules related to SIMPLE presence extensions
set(MOD_LIST_PRESENCE
    presence
    presence_conference
    presence_dialoginfo
    presence_mwi
    presence_profile
    presence_reginfo
    presence_xml
    pua
    pua_bla
    pua_dialoginfo
    pua_reginfo
    pua_rpc
    pua_usrloc
    pua_xmpp
    rls
    xcap_client
    xcap_server
)

# * modules depending on lua library
set(MOD_LIST_LUA app_lua)

# * modules depending on perl library
set(MOD_LIST_PERLDEPS app_perl db_perlvdb)

# * modules depending on python library
set(MOD_LIST_PYTHON app_python)

# * modules depending on python3 library
set(MOD_LIST_PYTHON3 app_python3 app_python3s)

# * modules depending on libm (math library - standard system library)
set(MOD_LIST_JSDT app_jsdt)

# * modules depending on ruby library
set(MOD_LIST_RUBY app_ruby app_ruby_proc)

# * modules depending on geoip library
set(MOD_LIST_GEOIP geoip)

# * modules depending on new geoip library
set(MOD_LIST_GEOIP2 geoip2)

# * modules depending on sqlite library
set(MOD_LIST_SQLITE db_sqlite)

# * modules depending on oracle library
set(MOD_LIST_ORACLE db_oracle)

# * modules depending on json library
set(MOD_LIST_JSON json pua_json)

# * modules depending on json (+libevent) library
set(MOD_LIST_JSON_EVENT jsonrpcc)

# * modules depending on jansson library
set(MOD_LIST_JANSSON jansson acc_json)

# * modules depending on jansson (+libevent) library
set(MOD_LIST_JANSSON_EVENT janssonrpcc)

# * modules depending on redis library
set(MOD_LIST_REDIS db_redis ndb_redis topos_redis)

# * modules depending on mono library
set(MOD_LIST_MONO app_mono)

# * modules related to IMS extensions
set(MOD_LIST_IMS
    cdp
    cdp_avp
    ims_dialog
    ims_auth
    ims_isc
    ims_icscf
    ims_qos
    ims_registrar_pcscf
    ims_registrar_scscf
    ims_usrloc_pcscf
    ims_usrloc_scscf
    ims_charging
    ims_ocs
    ims_diameter_server
    ims_ipsec_pcscf
)

# * modules depending on osp toolkit library
set(MOD_LIST_OSP osp)

# * modules depending on java library
set(MOD_LIST_JAVA app_java)

# * modules depending on gzip library
set(MOD_LIST_GZCOMPRESS gzcompress)

# * modules depending on uuid library
set(MOD_LIST_UUID uuid)

# * modules depending on ev library
set(MOD_LIST_EV evapi)

# * modules depending on libjwt library
set(MOD_LIST_JWT jwt)

# * modules depending on libwebsockets library
set(MOD_LIST_LWSC lwsc)

# * modules depending on libstirshaken library
set(MOD_LIST_STIRSHAKEN stirshaken)

# * modules depending on kazoo/rabbitmq
set(MOD_LIST_KAZOO kazoo)

# * modules depending on mongodb
set(MOD_LIST_MONGODB db_mongodb ndb_mongodb)

# * modules depending on redis and event library
set(MOD_LIST_CNXCC cnxcc)

# * modules depending on erlang library
set(MOD_LIST_ERLANG erlang)

# * modules depending on systemd library
set(MOD_LIST_SYSTEMD log_systemd systemdops)

# * modules depending on libnsq (+libev libevbuffsock libcurl libjson-c) library
set(MOD_LIST_NSQ nsq)

# * modules depending on librabbitmq library
set(MOD_LIST_RABBITMQ rabbitmq)

# * modules depending on libphonenumber library
set(MOD_LIST_PHONENUM phonenum)

# * modules depending on rdkafka library
set(MOD_LIST_KAFKA kafka)

# * modules depending on mqtt mosquitto and ev library
set(MOD_LIST_MQTT mqtt)

# * modules depending on nats and ev library
set(MOD_LIST_NATS nats)

# * modules depending on ruxc library
set(MOD_LIST_RUXC ruxc)

# * modules depending on microhttpd library
set(MOD_LIST_MICROHTTPD microhttpd)

# * modules depending on nghttp2 library
set(MOD_LIST_NGHTTP2 nghttp2)

# * modules depending on libgcrypt library
set(MOD_LIST_GCRYPT gcrypt)

# * modules depending on secsipid library
set(MOD_LIST_SECSIPID secsipid secsipid_proc)

# * modules depending on oRTP and mediastreamer2 libraries
set(MOD_LIST_RTP_MEDIA_SERVER rtp_media_server)

# * all modules
set(MOD_LIST_ALL
    ${MOD_LIST_BASIC}
    ${MOD_LIST_EXTRA}
    ${MOD_LIST_DB}
    ${MOD_LIST_DBUID}
    ${MOD_LIST_DEVEL}
    ${MOD_LIST_PCRE}
    ${MOD_LIST_RADIUS}
    ${MOD_LIST_LDAP}
    ${MOD_LIST_MYSQL}
    ${MOD_LIST_POSTGRES}
    ${MOD_LIST_UNIXODBC}
    ${MOD_LIST_XMLDEPS}
    ${MOD_LIST_PERLDEPS}
    ${MOD_LIST_CPLC}
    ${MOD_LIST_XMPP}
    ${MOD_LIST_BERKELEY}
    ${MOD_LIST_UTILS}
    ${MOD_LIST_MEMCACHED}
    ${MOD_LIST_TLSDEPS}
    ${MOD_LIST_WEBSOCKET}
    ${MOD_LIST_SNMPSTATS}
    ${MOD_LIST_PRESENCE}
    ${MOD_LIST_LUA}
    ${MOD_LIST_PYTHON}
    ${MOD_LIST_PYTHON3}
    ${MOD_LIST_RUBY}
    ${MOD_LIST_GEOIP}
    ${MOD_LIST_SQLITE}
    ${MOD_LIST_JSON}
    ${MOD_LIST_JSON_EVENT}
    ${MOD_LIST_REDIS}
    ${MOD_LIST_MONO}
    ${MOD_LIST_IMS}
    ${MOD_LIST_CASSANDRA}
    ${MOD_LIST_ORACLE}
    ${MOD_LIST_OUTBOUND}
    ${MOD_LIST_OSP}
    ${MOD_LIST_JAVA}
    ${MOD_LIST_DNSSEC}
    ${MOD_LIST_SCTP}
    ${MOD_LIST_AUTHEPH}
    ${MOD_LIST_GZCOMPRESS}
    ${MOD_LIST_UUID}
    ${MOD_LIST_EV}
    ${MOD_LIST_KAZOO}
    ${MOD_LIST_MONGODB}
    ${MOD_LIST_CNXCC}
    ${MOD_LIST_JANSSON}
    ${MOD_LIST_JANSSON_EVENT}
    ${MOD_LIST_GEOIP2}
    ${MOD_LIST_ERLANG}
    ${MOD_LIST_SYSTEMD}
    ${MOD_LIST_HTTP_ASYNC}
    ${MOD_LIST_NSQ}
    ${MOD_LIST_RABBITMQ}
    ${MOD_LIST_JSDT}
    ${MOD_LIST_PHONENUM}
    ${MOD_LIST_KAFKA}
    ${MOD_LIST_MQTT}
    ${MOD_LIST_NATS}
    ${MOD_LIST_RUXC}
    ${MOD_LIST_SECSIPID}
    ${MOD_LIST_JWT}
    ${MOD_LIST_LWSC}
    ${MOD_LIST_STIRSHAKEN}
    ${MOD_LIST_TLSA}
    ${MOD_LIST_TLS_WOLFSSL}
    ${MOD_LIST_MICROHTTPD}
    ${MOD_LIST_NGHTTP2}
    ${MOD_LIST_GCRYPT}
    ${MOD_LIST_RTP_MEDIA_SERVER}
)

# sort the list
list(SORT MOD_LIST_ALL)

# --- Groups defined for source code compilation ###

# groups are sets of modules selected by compile target interest (should be
# built by combining lists)

# Modules in this group are the default compiled modules due to no external
# compile or link dependencies
set(MODULE_GROUP_ALL ${MOD_LIST_ALL})

set(MODULE_GROUP_DEFAULT ${MOD_LIST_BASIC} ${MOD_LIST_EXTRA} ${MOD_LIST_DB}
                         ${MOD_LIST_DBUID} ${MOD_LIST_DEVEL} ${MOD_LIST_JSDT}
)

# Modules in this group are the default compiled modules due to no
# internal/external compile or link dependencies
# module_group_standard=$(mod_list_basic) $(mod_list_extra) \ $(mod_list_devel)
# $(mod_list_jsdt)
set(MODULE_GROUP_STANDARD ${MOD_LIST_BASIC} ${MOD_LIST_EXTRA} ${MOD_LIST_DEVEL}
                          ${MOD_LIST_JSDT}
)

# Modules in this group are considered a standard part due to widespread usage,
# but they have dependencies that must be satisfied for compilation (e.g., lcr,
# radius, presence, tls, ...). module_group_common=$(mod_list_db)
# $(mod_list_dbuid) \ $(mod_list_pcre) $(mod_list_radius) \ $(mod_list_xmldeps)
# $(mod_list_presence) \ $(mod_list_tlsdeps)
set(MODULE_GROUP_COMMON
    ${MOD_LIST_DB}
    ${MOD_LIST_DBUID}
    ${MOD_LIST_PCRE}
    ${MOD_LIST_RADIUS}
    ${MOD_LIST_XMLDEPS}
    ${MOD_LIST_PRESENCE}
    ${MOD_LIST_TLSDEPS}
)

# For db use (db modules, excluding drivers) module_group_db=$(mod_list_db)
set(MODULE_GROUP_DB ${MOD_LIST_DB})

# For mysql module_group_mysql_driver=$(mod_list_mysql)
# module_group_mysql=$(module_group_mysql_driver) $(module_group_db)
set(MODULE_GROUP_MYSQL_DRIVER ${MOD_LIST_MYSQL})
set(MODULE_GROUP_MYSQL ${MODULE_GROUP_MYSQL_DRIVER} ${MODULE_GROUP_DB})

# For postgress module_group_postgres_driver=$(mod_list_postgres)
# module_group_postgres=$(module_group_postgres_driver) $(module_group_db)
set(MODULE_GROUP_POSTGRES_DRIVER ${MOD_LIST_POSTGRES})
set(MODULE_GROUP_POSTGRES ${MODULE_GROUP_POSTGRES_DRIVER} ${MODULE_GROUP_DB})

# For sqlite module_group_sqlite_driver=$(mod_list_sqlite)
# module_group_sqlite=$(module_group_sqlite_driver) $(module_group_db)
set(MODULE_GROUP_SQLITE_DRIVER ${MOD_LIST_SQLITE})
set(MODULE_GROUP_SQLITE ${MODULE_GROUP_SQLITE_DRIVER} ${MODULE_GROUP_DB})

# For radius module_group_radius=$(mod_list_radius)

# For presence kamailio modules module_group_presence=$(mod_list_presence)

# For cassandra module_group_cassandra_driver=$(mod_list_cassandra)
# module_group_cassandra=$(module_group_cassandra_driver) $(module_group_db)

# For all modules not compiled by default module_group_ignore= $(sort
# $(filter-out $(module_group_default), $(mod_list_all)))

set(AVAILABLE_GROUPS
    ALL
    DEFAULT
    STANDARD
    COMMON
    # MODULE_GROUP_DB
    # MODULE_GROUP_MYSQL_DRIVER
    # MODULE_GROUP_MYSQL
    # MODULE_GROUP_POSTGRES_DRIVER
    # MODULE_GROUP_POSTGRES
    # MODULE_GROUP_SQLITE_DRIVER
    # MODULE_GROUP_SQLITE
    # MODULE_GROUP_RADIUS
    # module_group_presence
)
# --- Groups defined for pacKaging ###

# Standard modules in main pkg module_group_kstandard=$(mod_list_basic)
# $(mod_list_extra) \ $(mod_list_db) $(mod_list_dbuid) \ $(mod_list_pcre)
# $(mod_list_jsdt)

# Standard modules without any dependencies (such as pcre)
# module_group_kmini=$(mod_list_basic) $(mod_list_extra) \ $(mod_list_db)
# $(mod_list_dbuid) \ $(mod_list_jsdt)

# pkg pcre module module_group_kpcre=$(mod_list_pcre)

# pkg mysql module module_group_kmysql=$(mod_list_mysql)

# pkg postgress module module_group_kpostgres=$(mod_list_postgres)

# pkg cpl module module_group_kcpl=$(mod_list_cpl)

# pkg xml modules module_group_kxml=$(mod_list_xmldeps)

# pkg radius modules module_group_kradius=$(mod_list_radius)

# pkg unixodbc module module_group_kunixodbc=$(mod_list_unixodbc)

# pkg perl module module_group_kperl=$(mod_list_perldeps)

# pkg snmpstats module module_group_ksnmpstats=$(mod_list_snmpstats)

# pkg xmpp module module_group_kxmpp=$(mod_list_xmpp)

# pkg berkeley module module_group_kberkeley=$(mod_list_berkeley)

# pkg ldap modules module_group_kldap=$(mod_list_ldap)

# pkg utils module module_group_kutils=$(mod_list_utils)

# pkg https_async module module_group_khttp_async=$(mod_list_http_async)

# pkg memcached module module_group_kmemcached=$(mod_list_memcached)

# pkg tls module module_group_ktls_basic=$(mod_list_tlsdeps)

# ifeq ($(KTLS_INCLUDE_TLSA),yes) pkg tls module with curl
# module_group_ktls=$(mod_list_tlsdeps) $(mod_list_tlsa) else pkg tls module
# with curl module_group_ktls=$(mod_list_tlsdeps)

# pkg tlsa module module_group_ktlsa=$(mod_list_tlsa) endif

# pkg tls_wolfssl module module_group_ktls_wolfssl=$(mod_list_tls_wolfssl)

# pkg websocket module module_group_kwebsocket=$(mod_list_websocket)

# pkg presence modules module_group_kpresence=$(mod_list_presence)

# pkg lua module module_group_klua=$(mod_list_lua)

# pkg python module module_group_kpython=$(mod_list_python)

# pkg python3 module module_group_kpython3=$(mod_list_python3)

# pkg ruby module module_group_kruby=$(mod_list_ruby)

# pkg geoip module module_group_kgeoip=$(mod_list_geoip)

# pkg geoip2 module module_group_kgeoip2=$(mod_list_geoip2)

# pkg sqlite module module_group_ksqlite=$(mod_list_sqlite)

# K json modules module_group_kjson_basic=$(mod_list_json)

# K json modules with libevent module_group_kjson=$(mod_list_json)
# $(mod_list_json_event)

# K jansson modules module_group_kjansson_basic=$(mod_list_jansson)

# K jansson modules with libevent module_group_kjansson=$(mod_list_jansson)
# $(mod_list_jansson_event)

# pkg redis module module_group_kredis=$(mod_list_redis)

# pkg mono module module_group_kmono=$(mod_list_mono)

# pkg IMS modules module_group_kims=$(mod_list_ims)

# pkg outbound module module_group_koutbound=$(mod_list_outbound)

# pkg java module module_group_kjava=$(mod_list_java)

# pkg dnssec module module_group_kdnssec=$(mod_list_dnssec)

# pkg sctp module module_group_ksctp=$(mod_list_sctp)

# pkg auth_ephemeral module module_group_kautheph=$(mod_list_autheph)

# pkg gzcompress module module_group_kgzcompress=$(mod_list_gzcompress)

# pkg uuid module module_group_kuuid=$(mod_list_uuid)

# pkg libev modules module_group_kev=$(mod_list_ev)

# pkg jwt module module_group_kjwt=$(mod_list_jwt)

# pkg lwsc module module_group_klwsc=$(mod_list_lwsc)

# pkg stirshaken module module_group_kstirshaken=$(mod_list_stirshaken)

# pkg kazoo module module_group_kkazoo=$(mod_list_kazoo)

# pkg mongodb modules module_group_kmongodb=$(mod_list_mongodb)

# pkg cnxcc module module_group_kcnxcc=$(mod_list_cnxcc)

# pkg erlang module module_group_kerlang=$(mod_list_erlang)

# pkg systemd module module_group_ksystemd=$(mod_list_systemd)

# K nsq modules module_group_knsq=$(mod_list_nsq)

# K rabbitmq modules module_group_krabbitmq=$(mod_list_rabbitmq)

# K phonenumber modules module_group_kphonenum=$(mod_list_phonenum)

# K kafka modules module_group_kkafka=$(mod_list_kafka)

# K mqtt modules module_group_kmqtt=$(mod_list_mqtt)

# K nats modules module_group_knats=$(mod_list_nats)

# K ruxc modules module_group_kruxc=$(mod_list_ruxc)

# K microhttpd module module_group_kmicrohttpd=$(mod_list_microhttpd)

# K nghttp2 module module_group_knghttp2=$(mod_list_nghttp2)

# K gcrypt module module_group_kgcrypt=$(mod_list_gcrypt)

# K secsipid modules module_group_ksecsipid=$(mod_list_secsipid)

# K rtp_media_server modules
# module_group_krtp_media_server=$(mod_list_rtp_media_server)

# list of static modules
#
# static_modules:=
