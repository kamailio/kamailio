CREATE TABLE version (
    table_name string(32) NOT NULL,
    table_version int NOT NULL DEFAULT '0'
);

INSERT INTO version (table_name, table_version) VALUES ('acc', '3');
INSERT INTO version (table_name, table_version) VALUES ('missed_calls', '3');
INSERT INTO version (table_name, table_version) VALUES ('location', '8');
INSERT INTO version (table_name, table_version) VALUES ('credentials', '6');
INSERT INTO version (table_name, table_version) VALUES ('domain', '2');
INSERT INTO version (table_name, table_version) VALUES ('attr_types', '1');
INSERT INTO version (table_name, table_version) VALUES ('global_attrs', '1');
INSERT INTO version (table_name, table_version) VALUES ('domain_attrs', '1');
INSERT INTO version (table_name, table_version) VALUES ('user_attrs', '3');
INSERT INTO version (table_name, table_version) VALUES ('phonebook', '1');
INSERT INTO version (table_name, table_version) VALUES ('silo', '3');
INSERT INTO version (table_name, table_version) VALUES ('uri', '2');
INSERT INTO version (table_name, table_version) VALUES ('server_monitoring', '1');
INSERT INTO version (table_name, table_version) VALUES ('trusted', '1');
INSERT INTO version (table_name, table_version) VALUES ('server_monitoring_agg', '1');
INSERT INTO version (table_name, table_version) VALUES ('speed_dial', '2');
INSERT INTO version (table_name, table_version) VALUES ('sd_attrs', '1');
INSERT INTO version (table_name, table_version) VALUES ('gw', '2');
INSERT INTO version (table_name, table_version) VALUES ('gw_grp', '2');
INSERT INTO version (table_name, table_version) VALUES ('lcr', '1');
INSERT INTO version (table_name, table_version) VALUES ('presentity', '1');
INSERT INTO version (table_name, table_version) VALUES ('presentity_contact', '1');
INSERT INTO version (table_name, table_version) VALUES ('watcherinfo', '1');

CREATE TABLE acc (
    id int NOT NULL,
    from_uid string(64),
    to_uid string(64),
    to_did string(64),
    from_did string(64),
    sip_from string(255),
    sip_to string(255),
    sip_status string(128),
    sip_method string(16),
    in_ruri string(255),
    out_ruri string(255),
    from_uri string(255),
    to_uri string(255),
    sip_callid string(255),
    sip_cseq int,
    digest_username string(64),
    digest_realm string(255),
    from_tag string(128),
    to_tag string(128),
    src_ip int,
    src_port short,
    request_timestamp datetime NOT NULL,
    response_timestamp datetime NOT NULL,
    flags int NOT NULL DEFAULT '0',
    attrs string(255),
    id_key UNIQUE (id, ),

);

CREATE TABLE missed_calls (
    id int NOT NULL,
    from_uid string(64),
    to_uid string(64),
    to_did string(64),
    from_did string(64),
    sip_from string(255),
    sip_to string(255),
    sip_status string(128),
    sip_method string(16),
    inbound_ruri string(255),
    outbound_ruri string(255),
    from_uri string(255),
    to_uri string(255),
    sip_callid string(255),
    sip_cseq int,
    digest_username string(64),
    digest_realm string(255),
    from_tag string(128),
    to_tag string(128),
    request_timestamp datetime NOT NULL,
    response_timestamp datetime NOT NULL,
    flags int NOT NULL DEFAULT '0',
    attrs string(255),
    id_key UNIQUE (id, ),

);

CREATE TABLE credentials (
    auth_username string(64) NOT NULL,
    realm string(64) NOT NULL,
    password string(28) NOT NULL DEFAULT '',
    flags int NOT NULL DEFAULT '0',
    ha1 string(32) NOT NULL,
    ha1b string(32) NOT NULL DEFAULT '',
    uid string(64) NOT NULL,
     UNIQUE (auth_username, realm, ),

);

CREATE TABLE attr_types (
    name string(32) NOT NULL,
    rich_type string(32) NOT NULL DEFAULT 'string',
    raw_type int NOT NULL DEFAULT '2',
    type_spec string(255) DEFAULT NULL
);

INSERT INTO attr_types (name, raw_type) VALUES ('uid', '2');
INSERT INTO attr_types (name, raw_type) VALUES ('did', '2');
INSERT INTO attr_types (name, raw_type) VALUES ('digest_realm', '2');
INSERT INTO attr_types (name, raw_type) VALUES ('rpid', '2');
INSERT INTO attr_types (name, raw_type) VALUES ('fr_timer', '0');
INSERT INTO attr_types (name, raw_type) VALUES ('fr_inv_timer', '2');
INSERT INTO attr_types (name, raw_type) VALUES ('flags', '0');

CREATE TABLE global_attrs (
    name string(32) NOT NULL,
    type int NOT NULL DEFAULT '0',
    value string(64),
    flags int NOT NULL DEFAULT '0',
    global_attrs_idx UNIQUE (name, value, )
);

CREATE TABLE domain_attrs (
    did string(64),
    name string(32) NOT NULL,
    type int NOT NULL DEFAULT '0',
    value string(64),
    flags int NOT NULL DEFAULT '0',
    domain_attr_idx UNIQUE (did, name, value, ),

);

CREATE TABLE user_attrs (
    uid string(64) NOT NULL,
    name string(32) NOT NULL,
    value string(64),
    type int NOT NULL DEFAULT '0',
    flags int NOT NULL DEFAULT '0',
    userattrs_idx UNIQUE (uid, name, value, )
);

CREATE TABLE domain (
    did string(64) NOT NULL,
    domain string(128) NOT NULL,
    last_modified datetime NOT NULL,
    flags int NOT NULL DEFAULT '0',
    domain_idx UNIQUE (did, domain, )
);

CREATE TABLE location (
    uid string(64) NOT NULL,
    contact string(255) NOT NULL,
    received string(255),
    expires datetime NOT NULL DEFAULT '1970-01-01 00:00:00',
    q float NOT NULL DEFAULT '1.0',
    callid string(255),
    cseq int,
    flags int NOT NULL DEFAULT '0',
    user_agent string(64),
    location_key UNIQUE (uid, contact, ),

);

CREATE TABLE trusted (
    src_ip string(39) NOT NULL,
    proto string(4) NOT NULL,
    from_pattern string(64) NOT NULL,
    trusted_idx UNIQUE (src_ip, proto, from_pattern, )
);

CREATE TABLE server_monitoring (
    time datetime NOT NULL DEFAULT '1970-01-01 00:00:00',
    id int NOT NULL DEFAULT '0',
    param string(32) NOT NULL DEFAULT '',
    value int NOT NULL DEFAULT '0',
    increment int NOT NULL DEFAULT '0'
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
    lastupdate datetime NOT NULL DEFAULT '1970-01-01 00:00:00'
);

CREATE TABLE phonebook (
    id int NOT NULL,
    uid string(64) NOT NULL,
    fname string(32),
    lname string(32),
    sip_uri string(255) NOT NULL,
    pb_idx UNIQUE (id, ),

);

CREATE TABLE gw (
    gw_name string(128) NOT NULL,
    ip_addr int NOT NULL,
    port short,
    uri_scheme char,
    transport short,
    grp_id int NOT NULL,
    gw_idx1 UNIQUE (gw_name, ),

);

CREATE TABLE gw_grp (
    grp_id int NOT NULL,
    grp_name string(64) NOT NULL,
    gwgrp_idx UNIQUE (grp_id, )
);

CREATE TABLE lcr (
    prefix string(16) NOT NULL,
    from_uri string(255) NOT NULL DEFAULT '%',
    grp_id int,
    priority int
);

CREATE TABLE silo (
    mid int NOT NULL,
    src_addr string(255) NOT NULL,
    dst_addr string(255) NOT NULL,
    r_uri string(255) NOT NULL,
    uid string(64) NOT NULL,
    inc_time datetime NOT NULL DEFAULT '1970-01-01 00:00:00',
    exp_time datetime NOT NULL DEFAULT '1970-01-01 00:00:00',
    ctype string(128) NOT NULL DEFAULT 'text/plain',
    body binary NOT NULL DEFAULT '',
    silo_idx1 UNIQUE (mid, )
);

CREATE TABLE uri (
    uid string(64) NOT NULL,
    did string(64) NOT NULL,
    username string(64) NOT NULL,
    flags int NOT NULL DEFAULT '0',
    uri_idx1 UNIQUE (username, did, flags, ),
    uri_uid UNIQUE (uid, flags, )
);

CREATE TABLE speed_dial (
    id int NOT NULL,
    uid string(64) NOT NULL,
    dial_username string(64) NOT NULL,
    dial_did string(64) NOT NULL,
    new_uri string(255) NOT NULL,
    speeddial_idx1 UNIQUE (uid, dial_did, dial_username, ),
    speeddial_id UNIQUE (id, ),

);

CREATE TABLE sd_attrs (
    id string(64) NOT NULL,
    name string(32) NOT NULL,
    value string(64),
    type int NOT NULL DEFAULT '0',
    flags int NOT NULL DEFAULT '0',
    userattrs_idx UNIQUE (id, name, value, )
);

CREATE TABLE presentity (
    presid int(10) NOT NULL,
    uri string(255) NOT NULL,
    pdomain string(128) NOT NULL,
    presentity_key UNIQUE (presid, ),

);

CREATE TABLE presentity_contact (
    contactid int(10) NOT NULL,
    presid int(10) NOT NULL,
    basic string(32) NOT NULL DEFAULT 'offline',
    status string(32) NOT NULL,
    location string(128) NOT NULL,
    expires datetime NOT NULL DEFAULT '2020-05-28 21:32:15',
    placeid int(10),
    priority float NOT NULL DEFAULT '0.5',
    contact string(255),
    tupleid string(64) NOT NULL,
    prescaps int(10) NOT NULL,
    pc_idx1 UNIQUE (contactid, ),

);

CREATE TABLE watcherinfo (
    r_uri string(255) NOT NULL,
    w_uri string(255) NOT NULL,
    display_name string(128) NOT NULL,
    s_id string(64) NOT NULL,
    package string(32) NOT NULL DEFAULT 'presence',
    status string(32) NOT NULL DEFAULT 'pending',
    event string(32) NOT NULL,
    expires int NOT NULL,
    accepts int NOT NULL,
    presid int(10) NOT NULL,
    server_contact string(255) NOT NULL,
    dialog binary NOT NULL,
    doc_index int NOT NULL,
    wi_idx1 UNIQUE (s_id, ),

);

