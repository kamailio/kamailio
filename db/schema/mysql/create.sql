CREATE DATABASE ser;
USE ser;

CREATE TABLE version (
    table_name VARCHAR(32) NOT NULL,
    table_version INT UNSIGNED NOT NULL DEFAULT '0'
) Type=MyISAm;

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
    time DATETIME NOT NULL DEFAULT '0000-00-00 00:00:00',
    timestamp DATETIME NOT NULL DEFAULT '',
    caller_deleted TINYINT NOT NULL DEFAULT '',
    callee_deleted TINYINT NOT NULL DEFAULT '',
    KEY acc_user (username, domain),
    KEY sip_callid (sip_callid)
);

CREATE TABLE active_sessions (
    sid VARCHAR(32) NOT NULL DEFAULT '',
    name VARCHAR(32) NOT NULL DEFAULT '',
    val VARCHAR(32) NOT NULL DEFAULT '',
    changed VARCHAR(14) NOT NULL,
    KEY name (name, sid),
    KEY changed (changed)
);

CREATE TABLE aliases (
    username VARCHAR(64) NOT NULL DEFAULT '',
    domain VARCHAR(128) NOT NULL DEFAULT '',
    contact VARCHAR(255) NOT NULL DEFAULT '',
    received VARCHAR(255) DEFAULT NULL,
    expires DATETIME NOT NULL DEFAULT '1234',
    q FLOAT NOT NULL DEFAULT '1.0',
    callid VARCHAR(255) NOT NULL DEFAULT 'default_callid',
    cseq INT UNSIGNED NOT NULL DEFAULT '42',
    last_modified DATETIME NOT NULL DEFAULT '',
    replicate INT UNSIGNED NOT NULL DEFAULT '0',
    state INT UNSIGNED NOT NULL DEFAULT '0',
    flags INT UNSIGNED NOT NULL DEFAULT '0',
    user_agent VARCHAR(64) NOT NULL DEFAULT '',
    KEY main_key (username, domain, contact),
    KEY aliases_contact (contact)
);

CREATE TABLE event (
    id INT UNSIGNED NOT NULL,
    username VARCHAR(64) NOT NULL DEFAULT '',
    domain VARCHAR(128) NOT NULL DEFAULT '',
    uri VARCHAR(255) NOT NULL DEFAULT '',
    description VARCHAR(128) NOT NULL DEFAULT '',
    KEY (id)
);

CREATE TABLE grp (
    username VARCHAR(64) NOT NULL DEFAULT '',
    domain VARCHAR(128) NOT NULL DEFAULT '',
    grp VARCHAR(64) NOT NULL DEFAULT '',
    last_modified DATETIME NOT NULL DEFAULT '',
    KEY (username, domain, grp)
);

CREATE TABLE location (
    username VARCHAR(64) NOT NULL DEFAULT '',
    domain VARCHAR(128) NOT NULL DEFAULT '',
    contact VARCHAR(255) NOT NULL DEFAULT '',
    received VARCHAR(255) DEFAULT NULL,
    expires DATETIME NOT NULL DEFAULT '1234',
    q FLOAT NOT NULL DEFAULT '1.0',
    callid VARCHAR(255) NOT NULL DEFAULT 'default_callid',
    cseq INT UNSIGNED NOT NULL DEFAULT '42',
    last_modified DATETIME NOT NULL DEFAULT '',
    replicate INT UNSIGNED NOT NULL DEFAULT '0',
    state INT UNSIGNED NOT NULL DEFAULT '0',
    flags INT UNSIGNED NOT NULL DEFAULT '0',
    user_agent VARCHAR(64) NOT NULL DEFAULT '',
    KEY main_key (username, domain, contact),
    KEY aliases_contact (contact)
);

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
    time DATETIME NOT NULL DEFAULT '0',
    timestamp DATETIME NOT NULL DEFAULT '',
    KEY mc_user (username, domain)
);

CREATE TABLE pending (
    phplib_id VARCHAR(32) NOT NULL DEFAULT '',
    username VARCHAR(64) NOT NULL DEFAULT '',
    domain VARCHAR(128) NOT NULL DEFAULT '',
    password VARCHAR(25) NOT NULL DEFAULT '',
    first_name VARCHAR(25) NOT NULL DEFAULT '',
    last_name VARCHAR(45) NOT NULL DEFAULT '',
    phone VARCHAR(15) NOT NULL DEFAULT '',
    email_address VARCHAR(50) NOT NULL DEFAULT '',
    datetime_created DATETIME NOT NULL DEFAULT '0',
    datetime_modified DATETIME NOT NULL DEFAULT '0',
    confirmation VARCHAR(64) NOT NULL DEFAULT '',
    flag VARCHAR(1) NOT NULL DEFAULT 'o',
    sendnotification VARCHAR(50) NOT NULL DEFAULT '',
    greeting VARCHAR(50) NOT NULL DEFAULT '',
    ha1 VARCHAR(128) NOT NULL DEFAULT '',
    ha1b VARCHAR(128) NOT NULL DEFAULT '',
    allow_find VARCHAR(1) NOT NULL DEFAULT '',
    timezone VARCHAR(128) NOT NULL DEFAULT '',
    rpid VARCHAR(255) NOT NULL DEFAULT '',
    domn INT(10) UNSIGNED NOT NULL DEFAULT '',
    uuid VARCHAR(255) NOT NULL DEFAULT '',
    KEY (username, domain),
    KEY user_2 (username),
    KEY phplib_id (phplib_id)
);

CREATE TABLE phonebook (
    id INT UNSIGNED NOT NULL DEFAULT '',
    username VARCHAR(64) NOT NULL DEFAULT '',
    domain VARCHAR(128) NOT NULL DEFAULT '',
    fname VARCHAR(32) NOT NULL DEFAULT '',
    lname VARCHAR(32) NOT NULL DEFAULT '',
    sip_uri VARCHAR(255) NOT NULL DEFAULT '',
    KEY (id)
);

CREATE TABLE reserved (
    username VARCHAR(64) NOT NULL,
    UNIQUE KEY user2 (username)
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
    datetime_created DATETIME NOT NULL DEFAULT '0',
    datetime_modified DATETIME NOT NULL DEFAULT '0',
    confirmation VARCHAR(64) NOT NULL DEFAULT '',
    flag VARCHAR(1) NOT NULL DEFAULT 'o',
    sendnotification VARCHAR(50) NOT NULL DEFAULT '',
    greeting VARCHAR(50) NOT NULL DEFAULT '',
    ha1 VARCHAR(128) NOT NULL DEFAULT '',
    ha1b VARCHAR(128) NOT NULL DEFAULT '',
    allow_find VARCHAR(1) NOT NULL DEFAULT '',
    timezone VARCHAR(128) NOT NULL DEFAULT '',
    rpid VARCHAR(255) NOT NULL DEFAULT '',
    domn INT(10) UNSIGNED NOT NULL DEFAULT '',
    uuid VARCHAR(255) NOT NULL DEFAULT '',
    UNIQUE KEY (username, domain),
    KEY user_2 (username),
    KEY phplib_id (phplib_id)
);

CREATE TABLE config (
    attribute VARCHAR(32) NOT NULL,
    value VARCHAR(128) NOT NULL,
    username VARCHAR(64) NOT NULL DEFAULT '',
    domain VARCHAR(128) NOT NULL DEFAULT '',
    modified DATETIME
);

CREATE TABLE silo (
    mid INT UNSIGNED NOT NULL,
    src_addr VARCHAR(255) NOT NULL DEFAULT '',
    dst_addr VARCHAR(255) NOT NULL DEFAULT '',
    r_uri VARCHAR(255) NOT NULL DEFAULT '',
    username VARCHAR(64) NOT NULL DEFAULT '',
    domain VARCHAR(128) NOT NULL DEFAULT '',
    inc_time DATETIME NOT NULL DEFAULT '0',
    exp_time DATETIME NOT NULL DEFAULT '0',
    ctype VARCHAR(128) NOT NULL DEFAULT 'text/plain',
    body BLOB NOT NULL DEFAULT '',
    KEY (mid)
);

CREATE TABLE domain (
    domain VARCHAR(128) NOT NULL DEFAULT '',
    last_modified DATETIME NOT NULL DEFAULT '0',
    KEY (domain)
);

CREATE TABLE uri (
    username VARCHAR(64) NOT NULL DEFAULT '',
    domain VARCHAR(128) NOT NULL DEFAULT '',
    uri_user VARCHAR(64) NOT NULL DEFAULT '',
    last_modified DATETIME NOT NULL DEFAULT '',
    KEY (username, domain, uri_user)
);

CREATE TABLE server_monitoring (
    time DATETIME NOT NULL DEFAULT '0',
    id INT NOT NULL DEFAULT '0',
    param VARCHAR(32) NOT NULL DEFAULT '',
    value INT NOT NULL DEFAULT '0',
    increment INT NOT NULL DEFAULT '0',
    KEY (id, param)
);

CREATE TABLE usr_preferences (
    uuid VARCHAR(255) NOT NULL DEFAULT '',
    username VARCHAR(64) NOT NULL DEFAULT '',
    domain VARCHAR(128) NOT NULL DEFAULT '',
    attribute VARCHAR(32) NOT NULL DEFAULT '',
    value VARCHAR(128) NOT NULL DEFAULT '',
    type INT NOT NULL DEFAULT '0',
    modified DATETIME NOT NULL,
    KEY (attribute, username, domain)
);

CREATE TABLE usr_preferences_types (
    att_name VARCHAR(32) NOT NULL DEFAULT '',
    att_rich_type VARCHAR(32) NOT NULL DEFAULT 'string',
    att_raw_type INT NOT NULL DEFAULT '2',
    att_type_spec VARCHAR(255),
    default_value VARCHAR(100) NOT NULL DEFAULT '',
    KEY (att_name)
);

CREATE TABLE trusted (
    src_ip VARCHAR(39) NOT NULL,
    proto VARCHAR(4) NOT NULL,
    from_pattern VARCHAR(64) NOT NULL,
    KEY (src_ip, proto, from_pattern)
);

CREATE TABLE server_monitoring_agg (
    param VARCHAR(32) NOT NULL DEFAULT '',
    s_value INT NOT NULL DEFAULT '0',
    s_increment INT NOT NULL DEFAULT '0',
    last_aggregated_increment INT NOT NULL DEFAULT '0',
    av DOUBLE NOT NULL DEFAULT '0',
    mv INT NOT NULL DEFAULT '0',
    ad DOUBLE NOT NULL DEFAULT '0',
    lv INT NOT NULL DEFAULT '0',
    min_val INT NOT NULL DEFAULT '0',
    max_val INT NOT NULL DEFAULT '0',
    min_inc INT NOT NULL DEFAULT '0',
    max_inc INT NOT NULL DEFAULT '0',
    lastupdate DATETIME NOT NULL DEFAULT '0',
    KEY (param)
);

CREATE TABLE admin_privileges (
    username VARCHAR(64) NOT NULL DEFAULT '',
    domain VARCHAR(128) NOT NULL DEFAULT '',
    priv_name VARCHAR(64) NOT NULL DEFAULT '',
    priv_value VARCHAR(64) NOT NULL DEFAULT '0',
    KEY (username, priv_name, priv_value, domain)
);

CREATE TABLE call_forwarding (
    username VARCHAR(64) NOT NULL DEFAULT '',
    domain VARCHAR(128) NOT NULL DEFAULT '',
    uri_re VARCHAR(128) NOT NULL DEFAULT '',
    purpose VARCHAR(32) NOT NULL DEFAULT '',
    action VARCHAR(32) NOT NULL DEFAULT '',
    param1 VARCHAR(128) DEFAULT '',
    param2 VARCHAR(128) DEFAULT '',
    UNIQUE KEY cf_key (username, domain, uri_re, purpose)
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
    description VARCHAR(64) NOT NULL DEFAULT '',
    KEY (username, domain, sd_username, sd_domain)
);

CREATE TABLE gw (
    gw_name VARCHAR(128) NOT NULL,
    ip_addr INT UNSIGNED NOT NULL,
    port SMALLINT UNSIGNED,
    uri_scheme TINYINT UNSIGNED,
    transport SMALLINT UNSIGNED,
    grp_id INT NOT NULL,
    KEY (gw_name),
    KEY (grp_id)
);

CREATE TABLE gw_grp (
    grp_id INT NOT NULL,
    grp_name VARCHAR(64) NOT NULL
);

CREATE TABLE lcr (
    prefix VARCHAR(16) NOT NULL,
    from_uri INT(255) NOT NULL DEFAULT '%',
    grp_id INT,
    priority INT,
    KEY (prefix),
    KEY (from_uri),
    KEY (grp_id)
);

