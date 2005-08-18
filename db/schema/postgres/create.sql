CREATE DATABASE pg_ser;
USE pg_ser;

CREATE TABLE version (
    table_name string(32) NOT NULL,
    table_version int NOT NULL DEFAULT '0'
);

INSERT INTO version (table_name) VALUES ('acc');
INSERT INTO version (table_name, table_version) VALUES ('active_sessions', '1');
INSERT INTO version (table_name, table_version) VALUES ('aliases', '6');
INSERT INTO version (table_name, table_version) VALUES ('event', '1');
INSERT INTO version (table_name, table_version) VALUES ('grp', '2');
INSERT INTO version (table_name, table_version) VALUES ('location', '6');
INSERT INTO version (table_name, table_version) VALUES ('missed_calls', '2');
INSERT INTO version (table_name, table_version) VALUES ('pending', '4');
INSERT INTO version (table_name, table_version) VALUES ('phonebook', '1');
INSERT INTO version (table_name, table_version) VALUES ('reserved', '1');
INSERT INTO version (table_name, table_version) VALUES ('subscriber', '5');
INSERT INTO version (table_name, table_version) VALUES ('config', '1');
INSERT INTO version (table_name, table_version) VALUES ('silo', '3');
INSERT INTO version (table_name, table_version) VALUES ('domain', '1');
INSERT INTO version (table_name, table_version) VALUES ('uri', '1');
INSERT INTO version (table_name, table_version) VALUES ('server_monitoring', '1');
INSERT INTO version (table_name, table_version) VALUES ('usr_preferences', '2');
INSERT INTO version (table_name, table_version) VALUES ('usr_preferences_types', '1');
INSERT INTO version (table_name, table_version) VALUES ('trusted', '1');
INSERT INTO version (table_name, table_version) VALUES ('server_monitoring_agg', '1');
INSERT INTO version (table_name, table_version) VALUES ('admin_privileges', '1');
INSERT INTO version (table_name, table_version) VALUES ('call_forwarding', '1');
INSERT INTO version (table_name, table_version) VALUES ('speed_dial', '2');
INSERT INTO version (table_name, table_version) VALUES ('gw', '2');
INSERT INTO version (table_name, table_version) VALUES ('gw_grp', '2');
INSERT INTO version (table_name, table_version) VALUES ('lcr', '1');

CREATE TABLE acc (
    caller_UUID string(255) NOT NULL,
    callee_UUID string(255) NOT NULL,
    sip_from string(255) NOT NULL,
    sip_to string(255) NOT NULL,
    sip_status string(128) NOT NULL,
    sip_method string(16) NOT NULL,
    i_uri string(255) NOT NULL,
    o_uri string(255) NOT NULL,
    from_uri string(255) NOT NULL,
    to_uri string(255) NOT NULL,
    sip_callid string(255) NOT NULL,
    username string(64) NOT NULL,
    domain string(128) NOT NULL,
    fromtag string(128) NOT NULL,
    totag string(128) NOT NULL,
    time datetime NOT NULL DEFAULT '0000-00-00 00:00:00',
    timestamp datetime NOT NULL DEFAULT '',
    caller_deleted char NOT NULL DEFAULT '',
    callee_deleted char NOT NULL DEFAULT ''
);

CREATE TABLE active_sessions (
    sid string(32) NOT NULL DEFAULT '',
    name string(32) NOT NULL DEFAULT '',
    val string(32) NOT NULL DEFAULT '',
    changed string(14) NOT NULL
);

CREATE TABLE aliases (
    username string(64) NOT NULL DEFAULT '',
    domain string(128) NOT NULL DEFAULT '',
    contact string(255) NOT NULL DEFAULT '',
    received string(255) DEFAULT NULL,
    expires datetime NOT NULL DEFAULT '1234',
    q float NOT NULL DEFAULT '1.0',
    callid string(255) NOT NULL DEFAULT 'default_callid',
    cseq int NOT NULL DEFAULT '42',
    last_modified datetime NOT NULL DEFAULT '',
    replicate int NOT NULL DEFAULT '0',
    state int NOT NULL DEFAULT '0',
    flags int NOT NULL DEFAULT '0',
    user_agent string(64) NOT NULL DEFAULT ''
);

CREATE TABLE event (
    id int NOT NULL,
    username string(64) NOT NULL DEFAULT '',
    domain string(128) NOT NULL DEFAULT '',
    uri string(255) NOT NULL DEFAULT '',
    description string(128) NOT NULL DEFAULT ''
);

CREATE TABLE grp (
    username string(64) NOT NULL DEFAULT '',
    domain string(128) NOT NULL DEFAULT '',
    grp string(64) NOT NULL DEFAULT '',
    last_modified datetime NOT NULL DEFAULT ''
);

CREATE TABLE location (
    username string(64) NOT NULL DEFAULT '',
    domain string(128) NOT NULL DEFAULT '',
    contact string(255) NOT NULL DEFAULT '',
    received string(255) DEFAULT NULL,
    expires datetime NOT NULL DEFAULT '1234',
    q float NOT NULL DEFAULT '1.0',
    callid string(255) NOT NULL DEFAULT 'default_callid',
    cseq int NOT NULL DEFAULT '42',
    last_modified datetime NOT NULL DEFAULT '',
    replicate int NOT NULL DEFAULT '0',
    state int NOT NULL DEFAULT '0',
    flags int NOT NULL DEFAULT '0',
    user_agent string(64) NOT NULL DEFAULT ''
);

CREATE TABLE missed_calls (
    sip_from string(255) NOT NULL DEFAULT '',
    sip_to string(255) NOT NULL DEFAULT '',
    sip_status string(128) NOT NULL DEFAULT '',
    sip_method string(16) NOT NULL DEFAULT '',
    i_uri string(255) NOT NULL DEFAULT '',
    o_uri string(255) NOT NULL DEFAULT '',
    from_uri string(255) NOT NULL DEFAULT '',
    to_uri string(255) NOT NULL DEFAULT '',
    sip_callid string(255) NOT NULL DEFAULT '',
    username string(64) NOT NULL DEFAULT '',
    domain string(128) NOT NULL DEFAULT '',
    fromtag string(128) NOT NULL DEFAULT '',
    totag string(128) NOT NULL DEFAULT '',
    time datetime NOT NULL DEFAULT '0',
    timestamp datetime NOT NULL DEFAULT ''
);

CREATE TABLE pending (
    phplib_id string(32) NOT NULL DEFAULT '',
    username string(64) NOT NULL DEFAULT '',
    domain string(128) NOT NULL DEFAULT '',
    password string(25) NOT NULL DEFAULT '',
    first_name string(25) NOT NULL DEFAULT '',
    last_name string(45) NOT NULL DEFAULT '',
    phone string(15) NOT NULL DEFAULT '',
    email_address string(50) NOT NULL DEFAULT '',
    datetime_created datetime NOT NULL DEFAULT '0',
    datetime_modified datetime NOT NULL DEFAULT '0',
    confirmation string(64) NOT NULL DEFAULT '',
    flag string(1) NOT NULL DEFAULT 'o',
    sendnotification string(50) NOT NULL DEFAULT '',
    greeting string(50) NOT NULL DEFAULT '',
    ha1 string(128) NOT NULL DEFAULT '',
    ha1b string(128) NOT NULL DEFAULT '',
    allow_find string(1) NOT NULL DEFAULT '',
    timezone string(128) NOT NULL DEFAULT '',
    rpid string(255) NOT NULL DEFAULT '',
    domn int(10) NOT NULL DEFAULT '',
    uuid string(255) NOT NULL DEFAULT ''
);

CREATE TABLE phonebook (
    id int NOT NULL DEFAULT '',
    username string(64) NOT NULL DEFAULT '',
    domain string(128) NOT NULL DEFAULT '',
    fname string(32) NOT NULL DEFAULT '',
    lname string(32) NOT NULL DEFAULT '',
    sip_uri string(255) NOT NULL DEFAULT ''
);

CREATE TABLE reserved (
    username string(64) NOT NULL,
    user2 UNIQUE (username, )
);

CREATE TABLE subscriber (
    phplib_id string(32) NOT NULL DEFAULT '',
    username string(64) NOT NULL DEFAULT '',
    domain string(128) NOT NULL DEFAULT '',
    password string(25) NOT NULL DEFAULT '',
    first_name string(25) NOT NULL DEFAULT '',
    last_name string(45) NOT NULL DEFAULT '',
    phone string(15) NOT NULL DEFAULT '',
    email_address string(50) NOT NULL DEFAULT '',
    datetime_created datetime NOT NULL DEFAULT '0',
    datetime_modified datetime NOT NULL DEFAULT '0',
    confirmation string(64) NOT NULL DEFAULT '',
    flag string(1) NOT NULL DEFAULT 'o',
    sendnotification string(50) NOT NULL DEFAULT '',
    greeting string(50) NOT NULL DEFAULT '',
    ha1 string(128) NOT NULL DEFAULT '',
    ha1b string(128) NOT NULL DEFAULT '',
    allow_find string(1) NOT NULL DEFAULT '',
    timezone string(128) NOT NULL DEFAULT '',
    rpid string(255) NOT NULL DEFAULT '',
    domn int(10) NOT NULL DEFAULT '',
    uuid string(255) NOT NULL DEFAULT '',
     UNIQUE (username, domain, ),

);

CREATE TABLE config (
    attribute string(32) NOT NULL,
    value string(128) NOT NULL,
    username string(64) NOT NULL DEFAULT '',
    domain string(128) NOT NULL DEFAULT '',
    modified datetime
);

CREATE TABLE silo (
    mid int NOT NULL,
    src_addr string(255) NOT NULL DEFAULT '',
    dst_addr string(255) NOT NULL DEFAULT '',
    r_uri string(255) NOT NULL DEFAULT '',
    username string(64) NOT NULL DEFAULT '',
    domain string(128) NOT NULL DEFAULT '',
    inc_time datetime NOT NULL DEFAULT '0',
    exp_time datetime NOT NULL DEFAULT '0',
    ctype string(128) NOT NULL DEFAULT 'text/plain',
    body binary NOT NULL DEFAULT ''
);

CREATE TABLE domain (
    domain string(128) NOT NULL DEFAULT '',
    last_modified datetime NOT NULL DEFAULT '0'
);

CREATE TABLE uri (
    username string(64) NOT NULL DEFAULT '',
    domain string(128) NOT NULL DEFAULT '',
    uri_user string(64) NOT NULL DEFAULT '',
    last_modified datetime NOT NULL DEFAULT ''
);

CREATE TABLE server_monitoring (
    time datetime NOT NULL DEFAULT '0',
    id int NOT NULL DEFAULT '0',
    param string(32) NOT NULL DEFAULT '',
    value int NOT NULL DEFAULT '0',
    increment int NOT NULL DEFAULT '0'
);

CREATE TABLE usr_preferences (
    uuid string(255) NOT NULL DEFAULT '',
    username string(64) NOT NULL DEFAULT '',
    domain string(128) NOT NULL DEFAULT '',
    attribute string(32) NOT NULL DEFAULT '',
    value string(128) NOT NULL DEFAULT '',
    type int NOT NULL DEFAULT '0',
    modified datetime NOT NULL
);

CREATE TABLE usr_preferences_types (
    att_name string(32) NOT NULL DEFAULT '',
    att_rich_type string(32) NOT NULL DEFAULT 'string',
    att_raw_type int NOT NULL DEFAULT '2',
    att_type_spec string(255),
    default_value string(100) NOT NULL DEFAULT ''
);

CREATE TABLE trusted (
    src_ip string(39) NOT NULL,
    proto string(4) NOT NULL,
    from_pattern string(64) NOT NULL
);

CREATE TABLE server_monitoring_agg (
    param string(32) NOT NULL DEFAULT '',
    s_value int NOT NULL DEFAULT '0',
    s_increment int NOT NULL DEFAULT '0',
    last_aggregated_increment int NOT NULL DEFAULT '0',
    av double NOT NULL DEFAULT '0',
    mv int NOT NULL DEFAULT '0',
    ad double NOT NULL DEFAULT '0',
    lv int NOT NULL DEFAULT '0',
    min_val int NOT NULL DEFAULT '0',
    max_val int NOT NULL DEFAULT '0',
    min_inc int NOT NULL DEFAULT '0',
    max_inc int NOT NULL DEFAULT '0',
    lastupdate datetime NOT NULL DEFAULT '0'
);

CREATE TABLE admin_privileges (
    username string(64) NOT NULL DEFAULT '',
    domain string(128) NOT NULL DEFAULT '',
    priv_name string(64) NOT NULL DEFAULT '',
    priv_value string(64) NOT NULL DEFAULT '0'
);

CREATE TABLE call_forwarding (
    username string(64) NOT NULL DEFAULT '',
    domain string(128) NOT NULL DEFAULT '',
    uri_re string(128) NOT NULL DEFAULT '',
    purpose string(32) NOT NULL DEFAULT '',
    action string(32) NOT NULL DEFAULT '',
    param1 string(128) DEFAULT '',
    param2 string(128) DEFAULT '',
    cf_key UNIQUE (username, domain, uri_re, purpose, )
);

CREATE TABLE speed_dial (
    uuid string(255) NOT NULL DEFAULT '',
    username string(64) NOT NULL DEFAULT '',
    domain string(128) NOT NULL DEFAULT '',
    sd_username string(64) NOT NULL DEFAULT '',
    sd_domain string(128) NOT NULL DEFAULT '',
    new_uri string(255) NOT NULL DEFAULT '',
    fname string(128) NOT NULL DEFAULT '',
    lname string(128) NOT NULL DEFAULT '',
    description string(64) NOT NULL DEFAULT ''
);

CREATE TABLE gw (
    gw_name string(128) NOT NULL,
    ip_addr int NOT NULL,
    port short,
    uri_scheme char,
    transport short,
    grp_id int NOT NULL
);

CREATE TABLE gw_grp (
    grp_id int NOT NULL,
    grp_name string(64) NOT NULL
);

CREATE TABLE lcr (
    prefix string(16) NOT NULL,
    from_uri int(255) NOT NULL DEFAULT '%',
    grp_id int,
    priority int
);

