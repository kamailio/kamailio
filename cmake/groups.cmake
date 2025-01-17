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
    topos_htable
    sipdump
    pv_headers
    kemix)

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
    influxdbc)

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
    secfilter)

# * common modules depending on database, using UID db schema
set(MOD_LIST_DBUID db2_ops uid_auth_db uid_avp_db uid_domain uid_gflags
                   uid_uri_db)

# * modules for devel purposes
set(MOD_LIST_DEVEL misctest)

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
    presence_dfks
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
    xcap_server)

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
    ims_ipsec_pcscf)

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
    ${MOD_LIST_IMS}
    ${MOD_LIST_ORACLE}
    ${MOD_LIST_OUTBOUND}
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
    ${MOD_LIST_RTP_MEDIA_SERVER})

# sort the list
list(SORT MOD_LIST_ALL)

# --- Groups defined for source code compilation ###

# groups are sets of modules selected by compile target interest (should be
# built by combining lists)

# Modules in this group are the default compiled modules due to no external
# compile or link dependencies
set(MODULE_GROUP_ALL ${MOD_LIST_ALL})

set(MODULE_GROUP_DEFAULT ${MOD_LIST_BASIC} ${MOD_LIST_EXTRA} ${MOD_LIST_DB}
                         ${MOD_LIST_DBUID} ${MOD_LIST_DEVEL} ${MOD_LIST_JSDT})

# Modules in this group are the default compiled modules due to no
# internal/external compile or link dependencies
# module_group_standard=$(mod_list_basic) $(mod_list_extra) \ $(mod_list_devel)
# $(mod_list_jsdt)
set(MODULE_GROUP_STANDARD ${MOD_LIST_BASIC} ${MOD_LIST_EXTRA} ${MOD_LIST_DEVEL}
                          ${MOD_LIST_JSDT})

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
    ${MOD_LIST_TLSDEPS})

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

# For radius
set(MODULE_GROUP_RADIUS ${MODULE_LIST_RADIUS})

# For presence kamailio modules
set(MODULE_GROUP_PRESENCE ${MOD_LIST_PRESENCE})

# For cassandra
set(MODULE_GROUP_CASSANDRA_DRIVER ${MODULE_LIST_CASSANDRA})
set(MODULE_GROUP_CASSANDRA ${MODULE_GROUP_CASSANDRA_DRIVER} ${MODULE_GROUP_DB})

# For all modules not compiled by default
# list(FILTER MOD_LIST_ALL EXCLUDE REGEX "${MODULE_GROUP_DEFAULT}")

# for all protocols (excl. local ones such as unix, tcp, etc.)
set(AVAILABLE_GROUPS ALL DEFAULT STANDARD COMMON)

# --- Groups defined for pacKaging ###
# Standard modules in main pkg
set(MODULE_GROUP_KSTANDARD ${MOD_LIST_BASIC} ${MOD_LIST_EXTRA} ${MOD_LIST_DB}
                           ${MOD_LIST_DBUID} ${MOD_LIST_PCRE} ${MOD_LIST_JSDT})

# Standard modules without any dependencies (such as pcre)
set(MODULE_GROUP_KMINI ${MOD_LIST_BASIC} ${MOD_LIST_EXTRA} ${MOD_LIST_DB}
                       ${MOD_LIST_DBUID} ${MOD_LIST_JSDT})
# pkg pcre module
set(MODULE_GROUP_KPCRE ${MOD_LIST_PCRE})

# pkg mysql module
set(MODULE_GROUP_KMYSQL ${MOD_LIST_MYSQL})

# pkg postgress module
set(MODULE_GROUP_KPOSTGRES ${MOD_LIST_POSTGRES})

# pkg cpl module
set(MODULE_GROUP_KCPL ${MOD_LIST_CPL})

# pkg xml modules
set(MODULE_GROUP_KXML ${MOD_LIST_XMLDEPS})

# pkg radius modules
set(MODULE_GROUP_KRADIUS ${MOD_LIST_RADIUS})

# pkg unixodbc module
set(MODULE_GROUP_KUNIXODBC ${MOD_LIST_UNIXODBC})

# pkg perl module
set(MODULE_GROUP_KPERL ${MOD_LIST_PERLDEPS})

# pkg snmpstats module
set(MODULE_GROUP_KSNMPSTATS ${MOD_LIST_SNMPSTATS})

# pkg xmpp module
set(MODULE_GROUP_KXMPP ${MOD_LIST_XMPP})

# pkg berkeley module
set(MODULE_GROUP_KBERKELEY ${MOD_LIST_BERKELEY})

# pkg ldap modules
set(MODULE_GROUP_KLDAP ${MOD_LIST_LDAP})

# pkg utils module
set(MODULE_GROUP_KUTILS ${MOD_LIST_UTILS})

# pkg https_async module
set(MODULE_GROUP_KHTTP_ASYNC ${MOD_LIST_HTTP_ASYNC})

# pkg memcached module
set(MODULE_GROUP_KMEMCACHED ${MOD_LIST_MEMCACHED})

# pkg tls module
set(MODULE_GROUP_KTLS_BASIC ${MOD_LIST_TLSDEPS})

# pkg tls module with curl
if(KTLS_INCLUDE_TLSA)
  set(MODULE_GROUP_KTLS ${MOD_LIST_TLSDEPS} ${MOD_LIST_TLSA})
else()
  set(MODULE_GROUP_KTLS ${MOD_LIST_TLSDEPS})
  set(MODULE_GROUP_KTLSA ${MOD_LIST_TLSA})
endif()

# pkg tls_wolfssl module
set(MODULE_GROUP_KTLS_WOLFSSL ${MOD_LIST_TLS_WOLFSSL})

# pkg websocket module
set(MODULE_GROUP_KWEBSOCKET ${MOD_LIST_WEBSOCKET})

# pkg presence modules
set(MODULE_GROUP_KPRESENCE ${MOD_LIST_PRESENCE})

# pkg lua module
set(MODULE_GROUP_KLUA ${MOD_LIST_LUA})

# pkg python module
set(MODULE_GROUP_KPYTHON ${MOD_LIST_PYTHON})

# pkg python3 module
set(MODULE_GROUP_KPYTHON3 ${MOD_LIST_PYTHON3})

# pkg ruby module
set(MODULE_GROUP_KRUBY ${MOD_LIST_RUBY})

# pkg geoip module
set(MODULE_GROUP_KGEOIP ${MOD_LIST_GEOIP})

# pkg geoip2 module
set(MODULE_GROUP_KGEOIP2 ${MOD_LIST_GEOIP2})

# pkg sqlite module
set(MODULE_GROUP_KSQLITE ${MOD_LIST_SQLITE})

# K json modules
set(MODULE_GROUP_KJSON_BASIC ${MOD_LIST_JSON})

# K json modules with libevent
set(MODULE_GROUP_KJSON ${MOD_LIST_JSON} ${MOD_LIST_JSON_EVENT})

# K jansson modules
set(MODULE_GROUP_KJANSSON_BASIC ${MOD_LIST_JANSSON})

# K jansson modules with libevent
set(MODULE_GROUP_KJANSSON ${MOD_LIST_JANSSON} ${MOD_LIST_JANSSON_EVENT})

# pkg redis module
set(MODULE_GROUP_KREDIS ${MOD_LIST_REDIS})

# pkg IMS modules
set(MODULE_GROUP_KIMS ${MOD_LIST_IMS})

# pkg outbound module
set(MODULE_GROUP_KOUTBOUND ${MOD_LIST_OUTBOUND})

# pkg java module
set(MODULE_GROUP_KJAVA ${MOD_LIST_JAVA})

# pkg dnssec module
set(MODULE_GROUP_KDNSSEC ${MOD_LIST_DNSSEC})

# pkg sctp module
set(MODULE_GROUP_KSCTP ${MOD_LIST_SCTP})

# pkg auth_ephemeral module
set(MODULE_GROUP_KAUTHEPH ${MOD_LIST_AUTHEPH})

# pkg gzcompress module
set(MODULE_GROUP_KGZCOMPRESS ${MOD_LIST_GZCOMPRESS})

# pkg uuid module
set(MODULE_GROUP_KUUID ${MOD_LIST_UUID})

# pkg libev modules
set(MODULE_GROUP_KEV ${MOD_LIST_EV})

# pkg jwt module
set(MODULE_GROUP_KJWT ${MOD_LIST_JWT})

# pkg lwsc module
set(MODULE_GROUP_KLWSC ${MOD_LIST_LWSC})

# pkg stirshaken module
set(MODULE_GROUP_KSTIRSHAKEN ${MOD_LIST_STIRSHAKEN})

# pkg kazoo module
set(MODULE_GROUP_KKAZOO ${MOD_LIST_KAZOO})

# pkg mongodb modules
set(MODULE_GROUP_KMONGODB ${MOD_LIST_MONGODB})

# pkg cnxcc module
set(MODULE_GROUP_KCNXCC ${MOD_LIST_CNXCC})

# pkg erlang module
set(MODULE_GROUP_KERLANG ${MOD_LIST_ERLANG})

# pkg systemd module
set(MODULE_GROUP_KSYSTEMD ${MOD_LIST_SYSTEMD})

# K nsq modules
set(MODULE_GROUP_KNSQ ${MOD_LIST_NSQ})

# K rabbitmq modules
set(MODULE_GROUP_KRABBITMQ ${MOD_LIST_RABBITMQ})

# K phonenumber modules
set(MODULE_GROUP_KPHONENUM ${MOD_LIST_PHONENUM})

# K kafka modules
set(MODULE_GROUP_KKAFKA ${MOD_LIST_KAFKA})

# K mqtt modules
set(MODULE_GROUP_KMQTT ${MOD_LIST_MQTT})

# K nats modules
set(MODULE_GROUP_KNATS ${MOD_LIST_NATS})

# K ruxc modules
set(MODULE_GROUP_KRUXC ${MOD_LIST_RUXC})

# K microhttpd module
set(MODULE_GROUP_KMICROHTTPD ${MOD_LIST_MICROHTTPD})

# K nghttp2 module
set(MODULE_GROUP_KNGHTTP2 ${MOD_LIST_NGHTTP2})

# K gcrypt module
set(MODULE_GROUP_KGCRYPT ${MOD_LIST_GCRYPT})

# K secsipid modules
set(MODULE_GROUP_KSECSIPID ${MOD_LIST_SECSIPID})

# K rtp_media_server modules
set(MODULE_GROUP_KRTP_MEDIA_SERVER ${MOD_LIST_RTP_MEDIA_SERVER})

# list of static modules
set(STATIC_MODULES "")

list(
  APPEND
  AVAILABLE_GROUPS
  KSTANDARD
  KMINI
  KPCRE
  KMYSQL
  KPOSTGRES
  KCPL
  KXML
  KRAIDUS
  KUNIXODBC
  KPERL
  KSNMPSTATS
  KXMPP
  KBERKELEY
  KLDAP
  KUTILS
  KHTTP_ASYNC
  KMEMCACHED
  KTLS_BASIC
  KTLS
  KTLS_WOLFSSL
  KWEBSOCKET
  KPRESENCE
  KLUA
  KPYTHON
  KPYTHON3
  KRUBY
  KGEOIP
  KGEOIP2
  KSQLITE
  KJSON_BASIC
  KJSON
  KJANSSON_BASIC
  KJANSSON
  KREDIS
  KIMS
  KOUTBOUND
  KJAVA
  KDNSSEC
  KSCTP
  KAUTHEPH
  KGZCOMPRESS
  KUUID
  KEV
  KJWT
  KLWSC
  KSTIRSHAKEN
  KKAZOO
  KMONGODB
  KCNXCC
  KERLANG
  KSYSTEMD
  KNSQ
  KRABBITMQ
  KPHONENUM
  KKAFKA
  KMQTT
  KNATS
  KRUXC
  KMICROHTTPD
  KNGHTTP2
  KGCRYPT
  KSECSIPID
  KRT_MEDIA_SERVER)

# # Option to allow the user to define which group to build
# set(SELECTED_PACKAGE_GROUP
#     ""
#     CACHE STRING "Select the package group to build from"
#     PARENT_SCOPE
# )
# set_property(CACHE SELECTED_PACKAGE_GROUP PROPERTY STRINGS ${PACKAGE_GROUPS})

# # Ensure the selected group is valid
# if(NOT SELECTED_PACKAGE_GROUP IN_LIST PACKAGE_GROUPS)
#   message(
#     FATAL_ERROR
#       "Invalid package group selected: ${SELECTED_PACKAGE_GROUP}. Please choose from: ${PACKAGE_GROUPS}."
#   )
# endif()

# message(STATUS "Building package group: ${SELECTED_PACKAGE_GROUP}")
