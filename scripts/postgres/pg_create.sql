CREATE TABLE version (
    table_name VARCHAR(32) NOT NULL,
    table_version INTEGER NOT NULL DEFAULT '0'
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
    id SERIAL NOT NULL,
    from_uid VARCHAR(64),
    to_uid VARCHAR(64),
    to_did VARCHAR(64),
    from_did VARCHAR(64),
    sip_from VARCHAR(255),
    sip_to VARCHAR(255),
    sip_status VARCHAR(128),
    sip_method VARCHAR(16),
    in_ruri VARCHAR(255),
    out_ruri VARCHAR(255),
    from_uri VARCHAR(255),
    to_uri VARCHAR(255),
    sip_callid VARCHAR(255),
    sip_cseq INTEGER,
    digest_username VARCHAR(64),
    digest_realm VARCHAR(255),
    from_tag VARCHAR(128),
    to_tag VARCHAR(128),
    src_ip INTEGER,
    src_port SMALLINT,
    request_timestamp TIMESTAMP NOT NULL,
    response_timestamp TIMESTAMP NOT NULL,
    flags INTEGER NOT NULL DEFAULT '0',
    attrs VARCHAR(255),
    CONSTRAINT id_key UNIQUE (id)
);

CREATE INDEX cid_key ON acc (sip_callid);

CREATE TABLE missed_calls (
    id SERIAL NOT NULL,
    from_uid VARCHAR(64),
    to_uid VARCHAR(64),
    to_did VARCHAR(64),
    from_did VARCHAR(64),
    sip_from VARCHAR(255),
    sip_to VARCHAR(255),
    sip_status VARCHAR(128),
    sip_method VARCHAR(16),
    inbound_ruri VARCHAR(255),
    outbound_ruri VARCHAR(255),
    from_uri VARCHAR(255),
    to_uri VARCHAR(255),
    sip_callid VARCHAR(255),
    sip_cseq INTEGER,
    digest_username VARCHAR(64),
    digest_realm VARCHAR(255),
    from_tag VARCHAR(128),
    to_tag VARCHAR(128),
    request_timestamp TIMESTAMP NOT NULL,
    response_timestamp TIMESTAMP NOT NULL,
    flags INTEGER NOT NULL DEFAULT '0',
    attrs VARCHAR(255),
    CONSTRAINT id_key UNIQUE (id)
);

CREATE INDEX cid_key ON missed_calls (sip_callid);

CREATE TABLE credentials (
    auth_username VARCHAR(64) NOT NULL,
    realm VARCHAR(64) NOT NULL,
    password VARCHAR(28) NOT NULL DEFAULT '',
    flags INTEGER NOT NULL DEFAULT '0',
    ha1 VARCHAR(32) NOT NULL,
    ha1b VARCHAR(32) NOT NULL DEFAULT '',
    uid VARCHAR(64) NOT NULL,
    UNIQUE (auth_username, realm)
);

CREATE INDEX uid ON credentials (uid);

CREATE TABLE attr_types (
    name VARCHAR(32) NOT NULL,
    rich_type VARCHAR(32) NOT NULL DEFAULT 'string',
    raw_type INTEGER NOT NULL DEFAULT '2',
    type_spec VARCHAR(255) DEFAULT NULL
);

CREATE INDEX upt_idx1 ON attr_types (name);

INSERT INTO attr_types (name, raw_type) VALUES ('uid', '2');
INSERT INTO attr_types (name, raw_type) VALUES ('did', '2');
INSERT INTO attr_types (name, raw_type) VALUES ('digest_realm', '2');
INSERT INTO attr_types (name, raw_type) VALUES ('rpid', '2');
INSERT INTO attr_types (name, raw_type) VALUES ('fr_timer', '0');
INSERT INTO attr_types (name, raw_type) VALUES ('fr_inv_timer', '2');
INSERT INTO attr_types (name, raw_type) VALUES ('flags', '0');

CREATE TABLE global_attrs (
    name VARCHAR(32) NOT NULL,
    type INTEGER NOT NULL DEFAULT '0',
    value VARCHAR(64),
    flags INTEGER NOT NULL DEFAULT '0',
    CONSTRAINT global_attrs_idx UNIQUE (name, value)
);

CREATE TABLE domain_attrs (
    did VARCHAR(64),
    name VARCHAR(32) NOT NULL,
    type INTEGER NOT NULL DEFAULT '0',
    value VARCHAR(64),
    flags INTEGER NOT NULL DEFAULT '0',
    CONSTRAINT domain_attr_idx UNIQUE (did, name, value)
);

CREATE INDEX domain_did ON domain_attrs (did, flags);

CREATE TABLE user_attrs (
    uid VARCHAR(64) NOT NULL,
    name VARCHAR(32) NOT NULL,
    value VARCHAR(64),
    type INTEGER NOT NULL DEFAULT '0',
    flags INTEGER NOT NULL DEFAULT '0',
    CONSTRAINT userattrs_idx UNIQUE (uid, name, value)
);

CREATE TABLE domain (
    did VARCHAR(64) NOT NULL,
    domain VARCHAR(128) NOT NULL,
    last_modified TIMESTAMP NOT NULL,
    flags INTEGER NOT NULL DEFAULT '0',
    CONSTRAINT domain_idx UNIQUE (did, domain)
);

CREATE TABLE location (
    uid VARCHAR(64) NOT NULL,
    contact VARCHAR(255) NOT NULL,
    received VARCHAR(255),
    expires TIMESTAMP NOT NULL DEFAULT '1970-01-01 00:00:00',
    q REAL NOT NULL DEFAULT '1.0',
    callid VARCHAR(255),
    cseq INTEGER,
    flags INTEGER NOT NULL DEFAULT '0',
    user_agent VARCHAR(64),
    CONSTRAINT location_key UNIQUE (uid, contact)
);

CREATE INDEX location_contact ON location (contact);

CREATE TABLE trusted (
    src_ip VARCHAR(39) NOT NULL,
    proto VARCHAR(4) NOT NULL,
    from_pattern VARCHAR(64) NOT NULL,
    CONSTRAINT trusted_idx UNIQUE (src_ip, proto, from_pattern)
);

CREATE TABLE server_monitoring (
    time TIMESTAMP NOT NULL DEFAULT '1970-01-01 00:00:00',
    id INTEGER NOT NULL DEFAULT '0',
    param VARCHAR(32) NOT NULL DEFAULT '',
    value INTEGER NOT NULL DEFAULT '0',
    increment INTEGER NOT NULL DEFAULT '0'
);

CREATE INDEX sm_idx1 ON server_monitoring (id, param);

CREATE TABLE server_monitoring_agg (
    param VARCHAR(32) NOT NULL DEFAULT '',
    s_value INTEGER NOT NULL DEFAULT '0',
    s_increment INTEGER NOT NULL DEFAULT '0',
    last_aggregated_increment INTEGER NOT NULL DEFAULT '0',
    av DOUBLE PRECISION NOT NULL DEFAULT '0',
    mv INTEGER NOT NULL DEFAULT '0',
    ad DOUBLE PRECISION NOT NULL DEFAULT '0',
    lv INTEGER NOT NULL DEFAULT '0',
    min_val INTEGER NOT NULL DEFAULT '0',
    max_val INTEGER NOT NULL DEFAULT '0',
    min_inc INTEGER NOT NULL DEFAULT '0',
    max_inc INTEGER NOT NULL DEFAULT '0',
    lastupdate TIMESTAMP NOT NULL DEFAULT '1970-01-01 00:00:00'
);

CREATE INDEX smagg_idx1 ON server_monitoring_agg (param);

CREATE TABLE phonebook (
    id SERIAL NOT NULL,
    uid VARCHAR(64) NOT NULL,
    fname VARCHAR(32),
    lname VARCHAR(32),
    sip_uri VARCHAR(255) NOT NULL,
    CONSTRAINT pb_idx UNIQUE (id)
);

CREATE INDEX pb_uid ON phonebook (uid);

CREATE TABLE gw (
    gw_name VARCHAR(128) NOT NULL,
    ip_addr INTEGER NOT NULL,
    port SMALLINT,
    uri_scheme SMALLINT,
    transport SMALLINT,
    grp_id INTEGER NOT NULL,
    CONSTRAINT gw_idx1 UNIQUE (gw_name)
);

CREATE INDEX gw_idx2 ON gw (grp_id);

CREATE TABLE gw_grp (
    grp_id SERIAL NOT NULL,
    grp_name VARCHAR(64) NOT NULL,
    CONSTRAINT gwgrp_idx UNIQUE (grp_id)
);

CREATE TABLE lcr (
    prefix VARCHAR(16) NOT NULL,
    from_uri VARCHAR(255) NOT NULL DEFAULT '%',
    grp_id INTEGER,
    priority INTEGER
);

CREATE INDEX lcr_idx1 ON lcr (prefix);
CREATE INDEX lcr_idx2 ON lcr (from_uri);
CREATE INDEX lcr_idx3 ON lcr (grp_id);

CREATE TABLE silo (
    mid SERIAL NOT NULL,
    src_addr VARCHAR(255) NOT NULL,
    dst_addr VARCHAR(255) NOT NULL,
    r_uri VARCHAR(255) NOT NULL,
    uid VARCHAR(64) NOT NULL,
    inc_time TIMESTAMP NOT NULL DEFAULT '1970-01-01 00:00:00',
    exp_time TIMESTAMP NOT NULL DEFAULT '1970-01-01 00:00:00',
    ctype VARCHAR(128) NOT NULL DEFAULT 'text/plain',
    body BYTEA NOT NULL DEFAULT '',
    CONSTRAINT silo_idx1 UNIQUE (mid)
);

CREATE TABLE uri (
    uid VARCHAR(64) NOT NULL,
    did VARCHAR(64) NOT NULL,
    username VARCHAR(64) NOT NULL,
    flags INTEGER NOT NULL DEFAULT '0',
    CONSTRAINT uri_idx1 UNIQUE (username, did, flags),
    CONSTRAINT uri_uid UNIQUE (uid, flags)
);

CREATE TABLE speed_dial (
    id SERIAL NOT NULL,
    uid VARCHAR(64) NOT NULL,
    dial_username VARCHAR(64) NOT NULL,
    dial_did VARCHAR(64) NOT NULL,
    new_uri VARCHAR(255) NOT NULL,
    CONSTRAINT speeddial_idx1 UNIQUE (uid, dial_did, dial_username),
    CONSTRAINT speeddial_id UNIQUE (id)
);

CREATE INDEX speeddial_uid ON speed_dial (uid);

CREATE TABLE sd_attrs (
    id VARCHAR(64) NOT NULL,
    name VARCHAR(32) NOT NULL,
    value VARCHAR(64),
    type INTEGER NOT NULL DEFAULT '0',
    flags INTEGER NOT NULL DEFAULT '0',
    CONSTRAINT userattrs_idx UNIQUE (id, name, value)
);

CREATE TABLE presentity (
    presid SERIAL NOT NULL,
    uri VARCHAR(255) NOT NULL,
    pdomain VARCHAR(128) NOT NULL,
    CONSTRAINT presentity_key UNIQUE (presid)
);

CREATE INDEX presentity_key2 ON presentity (uri);

CREATE TABLE presentity_contact (
    contactid SERIAL NOT NULL,
    presid INTEGER NOT NULL,
    basic VARCHAR(32) NOT NULL DEFAULT 'offline',
    status VARCHAR(32) NOT NULL,
    location VARCHAR(128) NOT NULL,
    expires TIMESTAMP NOT NULL DEFAULT '2020-05-28 21:32:15',
    placeid INTEGER,
    priority REAL NOT NULL DEFAULT '0.5',
    contact VARCHAR(255),
    tupleid VARCHAR(64) NOT NULL,
    prescaps INTEGER NOT NULL,
    CONSTRAINT pc_idx1 UNIQUE (contactid)
);

CREATE INDEX presid_index ON presentity_contact (presid);
CREATE INDEX location_index ON presentity_contact (location);
CREATE INDEX placeid_index ON presentity_contact (placeid);

CREATE TABLE watcherinfo (
    r_uri VARCHAR(255) NOT NULL,
    w_uri VARCHAR(255) NOT NULL,
    display_name VARCHAR(128) NOT NULL,
    s_id VARCHAR(64) NOT NULL,
    package VARCHAR(32) NOT NULL DEFAULT 'presence',
    status VARCHAR(32) NOT NULL DEFAULT 'pending',
    event VARCHAR(32) NOT NULL,
    expires INTEGER NOT NULL,
    accepts INTEGER NOT NULL,
    presid INTEGER NOT NULL,
    server_contact VARCHAR(255) NOT NULL,
    dialog BYTEA NOT NULL,
    doc_index INTEGER NOT NULL,
    CONSTRAINT wi_idx1 UNIQUE (s_id)
);

CREATE INDEX wi_ruri_idx ON watcherinfo (r_uri);
CREATE INDEX wi_wuri_idx ON watcherinfo (w_uri);

