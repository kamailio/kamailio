CREATE TABLE version (
    table_name VARCHAR(32) NOT NULL,
    table_version INTEGER NOT NULL DEFAULT '0'
);

INSERT INTO version (table_name, table_version) VALUES ('acc', '2');
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
    caller_UUID VARCHAR(255) NOT NULL,
    callee_UUID VARCHAR(255) NOT NULL,
    sip_from VARCHAR(255) NOT NULL,
    sip_to VARCHAR(255) NOT NULL,
    sip_status VARCHAR(128) NOT NULL,
    sip_method VARCHAR(16) NOT NULL,
    i_uri VARCHAR(255) NOT NULL,
    o_uri VARCHAR(255) NOT NULL,
    from_uri VARCHAR(255) NOT NULL,
    to_uri VARCHAR(255) NOT NULL,
    sip_callid VARCHAR(255) NOT NULL,
    username VARCHAR(64) NOT NULL,
    domain VARCHAR(128) NOT NULL,
    fromtag VARCHAR(128) NOT NULL,
    totag VARCHAR(128) NOT NULL,
    time TIMESTAMP NOT NULL DEFAULT '1970-01-01 00:00:00',
    timestamp TIMESTAMP NOT NULL DEFAULT '1970-01-01 00:00:00',
    caller_deleted SMALLINT NOT NULL DEFAULT '0',
    callee_deleted SMALLINT NOT NULL DEFAULT '0'
);

CREATE INDEX acc_user ON acc (username, domain);
CREATE INDEX sip_callid ON acc (sip_callid);

CREATE TABLE active_sessions (
    sid VARCHAR(32) NOT NULL DEFAULT '',
    name VARCHAR(32) NOT NULL DEFAULT '',
    val VARCHAR(32) NOT NULL DEFAULT '',
    changed VARCHAR(14) NOT NULL
);

CREATE INDEX name ON active_sessions (name, sid);
CREATE INDEX changed ON active_sessions (changed);

CREATE TABLE aliases (
    username VARCHAR(64) NOT NULL DEFAULT '',
    domain VARCHAR(128) NOT NULL DEFAULT '',
    contact VARCHAR(255) NOT NULL DEFAULT '',
    received VARCHAR(255) DEFAULT NULL,
    expires TIMESTAMP NOT NULL DEFAULT '1970-01-01 00:00:00',
    q REAL NOT NULL DEFAULT '1.0',
    callid VARCHAR(255) NOT NULL DEFAULT 'default_callid',
    cseq INTEGER NOT NULL DEFAULT '42',
    last_modified TIMESTAMP NOT NULL DEFAULT '1970-01-01 00:00:00',
    replicate INTEGER NOT NULL DEFAULT '0',
    state INTEGER NOT NULL DEFAULT '0',
    flags INTEGER NOT NULL DEFAULT '0',
    user_agent VARCHAR(64) NOT NULL DEFAULT ''
);

CREATE INDEX main_key ON aliases (username, domain, contact);
CREATE INDEX aliases_contact ON aliases (contact);

CREATE TABLE event (
    id INTEGER NOT NULL,
    username VARCHAR(64) NOT NULL DEFAULT '',
    domain VARCHAR(128) NOT NULL DEFAULT '',
    uri VARCHAR(255) NOT NULL DEFAULT '',
    description VARCHAR(128) NOT NULL DEFAULT ''
);

CREATE INDEX id ON event (id);

CREATE TABLE grp (
    username VARCHAR(64) NOT NULL DEFAULT '',
    domain VARCHAR(128) NOT NULL DEFAULT '',
    grp VARCHAR(64) NOT NULL DEFAULT '',
    last_modified TIMESTAMP NOT NULL DEFAULT '1970-01-01 00:00:00'
);

CREATE INDEX grp_idx ON grp (username, domain, grp);

CREATE TABLE location (
    username VARCHAR(64) NOT NULL DEFAULT '',
    domain VARCHAR(128) NOT NULL DEFAULT '',
    contact VARCHAR(255) NOT NULL DEFAULT '',
    received VARCHAR(255) DEFAULT NULL,
    expires TIMESTAMP NOT NULL DEFAULT '1970-01-01 00:00:00',
    q REAL NOT NULL DEFAULT '1.0',
    callid VARCHAR(255) NOT NULL DEFAULT 'default_callid',
    cseq INTEGER NOT NULL DEFAULT '42',
    last_modified TIMESTAMP NOT NULL DEFAULT '1970-01-01 00:00:00',
    replicate INTEGER NOT NULL DEFAULT '0',
    state INTEGER NOT NULL DEFAULT '0',
    flags INTEGER NOT NULL DEFAULT '0',
    user_agent VARCHAR(64) NOT NULL DEFAULT ''
);

CREATE INDEX location_key ON location (username, domain, contact);
CREATE INDEX location_contact ON location (contact);

CREATE TABLE missed_calls (
    sip_from VARCHAR(255) NOT NULL DEFAULT '',
    sip_to VARCHAR(255) NOT NULL DEFAULT '',
    sip_status VARCHAR(128) NOT NULL DEFAULT '',
    sip_method VARCHAR(16) NOT NULL DEFAULT '',
    i_uri VARCHAR(255) NOT NULL DEFAULT '',
    o_uri VARCHAR(255) NOT NULL DEFAULT '',
    from_uri VARCHAR(255) NOT NULL DEFAULT '',
    to_uri VARCHAR(255) NOT NULL DEFAULT '',
    sip_callid VARCHAR(255) NOT NULL DEFAULT '',
    username VARCHAR(64) NOT NULL DEFAULT '',
    domain VARCHAR(128) NOT NULL DEFAULT '',
    fromtag VARCHAR(128) NOT NULL DEFAULT '',
    totag VARCHAR(128) NOT NULL DEFAULT '',
    time TIMESTAMP NOT NULL DEFAULT '1970-01-01 00:00:00',
    timestamp TIMESTAMP NOT NULL DEFAULT '1970-01-01 00:00:00'
);

CREATE INDEX mc_user ON missed_calls (username, domain);

CREATE TABLE pending (
    phplib_id VARCHAR(32) NOT NULL DEFAULT '',
    username VARCHAR(64) NOT NULL DEFAULT '',
    domain VARCHAR(128) NOT NULL DEFAULT '',
    password VARCHAR(25) NOT NULL DEFAULT '',
    first_name VARCHAR(25) NOT NULL DEFAULT '',
    last_name VARCHAR(45) NOT NULL DEFAULT '',
    phone VARCHAR(15) NOT NULL DEFAULT '',
    email_address VARCHAR(50) NOT NULL DEFAULT '',
    datetime_created TIMESTAMP NOT NULL DEFAULT '1970-01-01 00:00:00',
    datetime_modified TIMESTAMP NOT NULL DEFAULT '1970-01-01 00:00:00',
    confirmation VARCHAR(64) NOT NULL DEFAULT '',
    flag VARCHAR(1) NOT NULL DEFAULT 'o',
    sendnotification VARCHAR(50) NOT NULL DEFAULT '',
    greeting VARCHAR(50) NOT NULL DEFAULT '',
    ha1 VARCHAR(128) NOT NULL DEFAULT '',
    ha1b VARCHAR(128) NOT NULL DEFAULT '',
    allow_find VARCHAR(1) NOT NULL DEFAULT '',
    timezone VARCHAR(128) NOT NULL DEFAULT '',
    rpid VARCHAR(255) NOT NULL DEFAULT '',
    domn INTEGER NOT NULL DEFAULT '0',
    uuid VARCHAR(255) NOT NULL DEFAULT ''
);

CREATE INDEX pending_idx1 ON pending (username, domain);
CREATE INDEX user_2 ON pending (username);
CREATE INDEX php ON pending (phplib_id);

CREATE TABLE phonebook (
    id INTEGER NOT NULL DEFAULT '0',
    username VARCHAR(64) NOT NULL DEFAULT '',
    domain VARCHAR(128) NOT NULL DEFAULT '',
    fname VARCHAR(32) NOT NULL DEFAULT '',
    lname VARCHAR(32) NOT NULL DEFAULT '',
    sip_uri VARCHAR(255) NOT NULL DEFAULT ''
);

CREATE INDEX pb_idx ON phonebook (id);

CREATE TABLE reserved (
    username VARCHAR(64) NOT NULL,
    CONSTRAINT user2 UNIQUE (username)
);

CREATE TABLE subscriber (
    phplib_id VARCHAR(32) NOT NULL DEFAULT '',
    username VARCHAR(64) NOT NULL DEFAULT '',
    domain VARCHAR(128) NOT NULL DEFAULT '',
    password VARCHAR(25) NOT NULL DEFAULT '',
    first_name VARCHAR(25) NOT NULL DEFAULT '',
    last_name VARCHAR(45) NOT NULL DEFAULT '',
    phone VARCHAR(15) NOT NULL DEFAULT '',
    email_address VARCHAR(50) NOT NULL DEFAULT '',
    datetime_created TIMESTAMP NOT NULL DEFAULT '1970-01-01 00:00:00',
    datetime_modified TIMESTAMP NOT NULL DEFAULT '1970-01-01 00:00:00',
    confirmation VARCHAR(64) NOT NULL DEFAULT '',
    flag VARCHAR(1) NOT NULL DEFAULT 'o',
    sendnotification VARCHAR(50) NOT NULL DEFAULT '',
    greeting VARCHAR(50) NOT NULL DEFAULT '',
    ha1 VARCHAR(128) NOT NULL DEFAULT '',
    ha1b VARCHAR(128) NOT NULL DEFAULT '',
    allow_find VARCHAR(1) NOT NULL DEFAULT '',
    timezone VARCHAR(128) NOT NULL DEFAULT '',
    rpid VARCHAR(255) NOT NULL DEFAULT '',
    domn INTEGER NOT NULL DEFAULT '0',
    uuid VARCHAR(255) NOT NULL DEFAULT '',
    UNIQUE (username, domain)
);

CREATE INDEX sub_idx1 ON subscriber (username);
CREATE INDEX phplib_id ON subscriber (phplib_id);

CREATE TABLE config (
    attribute VARCHAR(32) NOT NULL,
    value VARCHAR(128) NOT NULL,
    username VARCHAR(64) NOT NULL DEFAULT '',
    domain VARCHAR(128) NOT NULL DEFAULT '',
    modified TIMESTAMP
);

CREATE TABLE silo (
    mid INTEGER NOT NULL,
    src_addr VARCHAR(255) NOT NULL DEFAULT '',
    dst_addr VARCHAR(255) NOT NULL DEFAULT '',
    r_uri VARCHAR(255) NOT NULL DEFAULT '',
    username VARCHAR(64) NOT NULL DEFAULT '',
    domain VARCHAR(128) NOT NULL DEFAULT '',
    inc_time TIMESTAMP NOT NULL DEFAULT '1970-01-01 00:00:00',
    exp_time TIMESTAMP NOT NULL DEFAULT '1970-01-01 00:00:00',
    ctype VARCHAR(128) NOT NULL DEFAULT 'text/plain',
    body BYTEA NOT NULL DEFAULT ''
);

CREATE INDEX silo_idx1 ON silo (mid);

CREATE TABLE domain (
    domain VARCHAR(128) NOT NULL DEFAULT '',
    last_modified TIMESTAMP NOT NULL DEFAULT '1970-01-01 00:00:00'
);

CREATE INDEX domain_idx1 ON domain (domain);

CREATE TABLE uri (
    username VARCHAR(64) NOT NULL DEFAULT '',
    domain VARCHAR(128) NOT NULL DEFAULT '',
    uri_user VARCHAR(64) NOT NULL DEFAULT '',
    last_modified TIMESTAMP NOT NULL DEFAULT '1970-01-01 00:00:00'
);

CREATE INDEX uri_idx1 ON uri (username, domain, uri_user);

CREATE TABLE server_monitoring (
    time TIMESTAMP NOT NULL DEFAULT '1970-01-01 00:00:00',
    id INTEGER NOT NULL DEFAULT '0',
    param VARCHAR(32) NOT NULL DEFAULT '',
    value INTEGER NOT NULL DEFAULT '0',
    increment INTEGER NOT NULL DEFAULT '0'
);

CREATE INDEX sm_idx1 ON server_monitoring (id, param);

CREATE TABLE usr_preferences (
    uuid VARCHAR(255) NOT NULL DEFAULT '',
    username VARCHAR(64) NOT NULL DEFAULT '',
    domain VARCHAR(128) NOT NULL DEFAULT '',
    attribute VARCHAR(32) NOT NULL DEFAULT '',
    value VARCHAR(128) NOT NULL DEFAULT '',
    type INTEGER NOT NULL DEFAULT '0',
    modified TIMESTAMP NOT NULL
);

CREATE INDEX up_idx ON usr_preferences (attribute, username, domain);

CREATE TABLE usr_preferences_types (
    att_name VARCHAR(32) NOT NULL DEFAULT '',
    att_rich_type VARCHAR(32) NOT NULL DEFAULT 'string',
    att_raw_type INTEGER NOT NULL DEFAULT '2',
    att_type_spec VARCHAR(255),
    default_value VARCHAR(100) NOT NULL DEFAULT ''
);

CREATE INDEX upt_idx1 ON usr_preferences_types (att_name);

CREATE TABLE trusted (
    src_ip VARCHAR(39) NOT NULL,
    proto VARCHAR(4) NOT NULL,
    from_pattern VARCHAR(64) NOT NULL
);

CREATE INDEX trusted_idx ON trusted (src_ip, proto, from_pattern);

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

CREATE TABLE admin_privileges (
    username VARCHAR(64) NOT NULL DEFAULT '',
    domain VARCHAR(128) NOT NULL DEFAULT '',
    priv_name VARCHAR(64) NOT NULL DEFAULT '',
    priv_value VARCHAR(64) NOT NULL DEFAULT '0'
);

CREATE INDEX adminpriv_idx1 ON admin_privileges (username, priv_name, priv_value, domain);

CREATE TABLE call_forwarding (
    username VARCHAR(64) NOT NULL DEFAULT '',
    domain VARCHAR(128) NOT NULL DEFAULT '',
    uri_re VARCHAR(128) NOT NULL DEFAULT '',
    purpose VARCHAR(32) NOT NULL DEFAULT '',
    action VARCHAR(32) NOT NULL DEFAULT '',
    param1 VARCHAR(128) DEFAULT '',
    param2 VARCHAR(128) DEFAULT '',
    CONSTRAINT cf_key UNIQUE (username, domain, uri_re, purpose)
);

CREATE TABLE speed_dial (
    uuid VARCHAR(255) NOT NULL DEFAULT '',
    username VARCHAR(64) NOT NULL DEFAULT '',
    domain VARCHAR(128) NOT NULL DEFAULT '',
    sd_username VARCHAR(64) NOT NULL DEFAULT '',
    sd_domain VARCHAR(128) NOT NULL DEFAULT '',
    new_uri VARCHAR(255) NOT NULL DEFAULT '',
    fname VARCHAR(128) NOT NULL DEFAULT '',
    lname VARCHAR(128) NOT NULL DEFAULT '',
    description VARCHAR(64) NOT NULL DEFAULT ''
);

CREATE INDEX speeddial_idx1 ON speed_dial (username, domain, sd_username, sd_domain);

CREATE TABLE gw (
    gw_name VARCHAR(128) NOT NULL,
    ip_addr INTEGER NOT NULL,
    port SMALLINT,
    uri_scheme SMALLINT,
    transport SMALLINT,
    grp_id INTEGER NOT NULL
);

CREATE INDEX gw_idx1 ON gw (gw_name);
CREATE INDEX gw_idx2 ON gw (grp_id);

CREATE TABLE gw_grp (
    grp_id INTEGER NOT NULL,
    grp_name VARCHAR(64) NOT NULL
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

