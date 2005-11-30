CREATE DATABASE ser;
USE ser;

CREATE TABLE version (
    table_name VARCHAR(32) NOT NULL,
    table_version INT UNSIGNED NOT NULL DEFAULT '0'
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
INSERT INTO version (table_name, table_version) VALUES ('silo', '4');
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
INSERT INTO version (table_name, table_version) VALUES ('i18n', '1');
INSERT INTO version (table_name, table_version) VALUES ('gw', '2');

CREATE TABLE acc (
    id INT AUTO_INCREMENT NOT NULL,
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
    sip_cseq INT,
    digest_username VARCHAR(64),
    digest_realm VARCHAR(255),
    from_tag VARCHAR(128),
    to_tag VARCHAR(128),
    src_ip INT UNSIGNED,
    src_port SMALLINT UNSIGNED,
    request_timestamp DATETIME NOT NULL,
    response_timestamp DATETIME NOT NULL,
    flags INT UNSIGNED NOT NULL DEFAULT '0',
    attrs VARCHAR(255),
    UNIQUE KEY id_key (id),
    KEY cid_key (sip_callid)
);

CREATE TABLE missed_calls (
    id INT AUTO_INCREMENT NOT NULL,
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
    sip_cseq INT,
    digest_username VARCHAR(64),
    digest_realm VARCHAR(255),
    from_tag VARCHAR(128),
    to_tag VARCHAR(128),
    request_timestamp DATETIME NOT NULL,
    response_timestamp DATETIME NOT NULL,
    flags INT UNSIGNED NOT NULL DEFAULT '0',
    attrs VARCHAR(255),
    UNIQUE KEY id_key (id),
    KEY cid_key (sip_callid)
);

CREATE TABLE credentials (
    auth_username VARCHAR(64) NOT NULL,
    realm VARCHAR(64) NOT NULL,
    password VARCHAR(28) NOT NULL DEFAULT '',
    flags INT NOT NULL DEFAULT '0',
    ha1 VARCHAR(32) NOT NULL,
    ha1b VARCHAR(32) NOT NULL DEFAULT '',
    uid VARCHAR(64) NOT NULL,
    KEY (auth_username, realm),
    KEY uid (uid)
);

CREATE TABLE attr_types (
    name VARCHAR(32) NOT NULL,
    rich_type VARCHAR(32) NOT NULL DEFAULT 'string',
    raw_type INT NOT NULL DEFAULT '2',
    type_spec VARCHAR(255) DEFAULT NULL,
    KEY upt_idx1 (name)
);

INSERT INTO attr_types (name, raw_type) VALUES ('uid', '2');
INSERT INTO attr_types (name, raw_type) VALUES ('did', '2');
INSERT INTO attr_types (name, raw_type) VALUES ('digest_realm', '2');
INSERT INTO attr_types (name, raw_type) VALUES ('rpid', '2');
INSERT INTO attr_types (name, raw_type) VALUES ('fr_timer', '0');
INSERT INTO attr_types (name, raw_type) VALUES ('fr_inv_timer', '2');
INSERT INTO attr_types (name, raw_type) VALUES ('flags', '0');
INSERT INTO attr_types (name, raw_type) VALUES ('gflags', '0');

CREATE TABLE global_attrs (
    name VARCHAR(32) NOT NULL,
    type INT NOT NULL DEFAULT '0',
    value VARCHAR(255),
    flags INT UNSIGNED NOT NULL DEFAULT '0',
    UNIQUE KEY global_attrs_idx (name, value)
);

CREATE TABLE domain_attrs (
    did VARCHAR(64),
    name VARCHAR(32) NOT NULL,
    type INT NOT NULL DEFAULT '0',
    value VARCHAR(255),
    flags INT UNSIGNED NOT NULL DEFAULT '0',
    UNIQUE KEY domain_attr_idx (did, name, value),
    KEY domain_did (did, flags)
);

CREATE TABLE user_attrs (
    uid VARCHAR(64) NOT NULL,
    name VARCHAR(32) NOT NULL,
    value VARCHAR(255),
    type INT NOT NULL DEFAULT '0',
    flags INT UNSIGNED NOT NULL DEFAULT '0',
    UNIQUE KEY userattrs_idx (uid, name, value)
);

CREATE TABLE domain (
    did VARCHAR(64) NOT NULL,
    domain VARCHAR(128) NOT NULL,
    last_modified timestamp NOT NULL,
    flags INT UNSIGNED NOT NULL DEFAULT '0',
    UNIQUE KEY domain_idx (did, domain)
);

CREATE TABLE location (
    uid VARCHAR(64) NOT NULL,
    contact VARCHAR(255) NOT NULL,
    received VARCHAR(255),
    expires DATETIME NOT NULL DEFAULT '1970-01-01 00:00:00',
    q FLOAT NOT NULL DEFAULT '1.0',
    callid VARCHAR(255),
    cseq INT UNSIGNED,
    flags INT UNSIGNED NOT NULL DEFAULT '0',
    user_agent VARCHAR(64),
    instance VARCHAR(255),
    UNIQUE KEY location_key (uid, contact),
    KEY location_contact (contact)
);

CREATE TABLE trusted (
    src_ip VARCHAR(39) NOT NULL,
    proto VARCHAR(4) NOT NULL,
    from_pattern VARCHAR(64) NOT NULL,
    UNIQUE KEY trusted_idx (src_ip, proto, from_pattern)
);

CREATE TABLE server_monitoring (
    time DATETIME NOT NULL DEFAULT '1970-01-01 00:00:00',
    id INT NOT NULL DEFAULT '0',
    param VARCHAR(32) NOT NULL DEFAULT '',
    value INT NOT NULL DEFAULT '0',
    increment INT NOT NULL DEFAULT '0',
    KEY sm_idx1 (id, param)
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
    lastupdate DATETIME NOT NULL DEFAULT '1970-01-01 00:00:00',
    KEY smagg_idx1 (param)
);

CREATE TABLE phonebook (
    id INT AUTO_INCREMENT NOT NULL,
    uid VARCHAR(64) NOT NULL,
    fname VARCHAR(32),
    lname VARCHAR(32),
    sip_uri VARCHAR(255) NOT NULL,
    UNIQUE KEY pb_idx (id),
    KEY pb_uid (uid)
);

CREATE TABLE gw (
    gw_name VARCHAR(128) NOT NULL,
    ip_addr INT UNSIGNED NOT NULL,
    port SMALLINT UNSIGNED,
    uri_scheme TINYINT UNSIGNED,
    transport SMALLINT UNSIGNED,
    grp_id INT NOT NULL,
    UNIQUE KEY gw_idx1 (gw_name),
    KEY gw_idx2 (grp_id)
);

CREATE TABLE gw_grp (
    grp_id INT AUTO_INCREMENT NOT NULL,
    grp_name VARCHAR(64) NOT NULL,
    UNIQUE KEY gwgrp_idx (grp_id)
);

CREATE TABLE lcr (
    prefix VARCHAR(16) NOT NULL,
    from_uri VARCHAR(255) NOT NULL DEFAULT '%',
    grp_id INT,
    priority INT,
    KEY lcr_idx1 (prefix),
    KEY lcr_idx2 (from_uri),
    KEY lcr_idx3 (grp_id)
);

CREATE TABLE grp (
    uid VARCHAR(64) NOT NULL DEFAULT '',
    grp VARCHAR(64) NOT NULL DEFAULT '',
    last_modified DATETIME NOT NULL DEFAULT '1970-01-01 00:00:00',
    KEY grp_idx (uid, grp)
);

CREATE TABLE silo (
    mid INT AUTO_INCREMENT NOT NULL,
    from VARCHAR(255) NOT NULL,
    to VARCHAR(255) NOT NULL,
    ruri VARCHAR(255) NOT NULL,
    uid VARCHAR(64) NOT NULL,
    inc_time DATETIME NOT NULL DEFAULT '1970-01-01 00:00:00',
    exp_time DATETIME NOT NULL DEFAULT '1970-01-01 00:00:00',
    ctype VARCHAR(128) NOT NULL DEFAULT 'text/plain',
    body BLOB NOT NULL DEFAULT '',
    UNIQUE KEY silo_idx1 (mid)
);

CREATE TABLE uri (
    uid VARCHAR(64) NOT NULL,
    did VARCHAR(64) NOT NULL,
    username VARCHAR(64) NOT NULL,
    flags INT UNSIGNED NOT NULL DEFAULT '0',
    UNIQUE KEY uri_idx1 (username, did, flags),
    UNIQUE KEY uri_uid (uid, flags)
);

CREATE TABLE speed_dial (
    id INT AUTO_INCREMENT NOT NULL,
    uid VARCHAR(64) NOT NULL,
    dial_username VARCHAR(64) NOT NULL,
    dial_did VARCHAR(64) NOT NULL,
    new_uri VARCHAR(255) NOT NULL,
    UNIQUE KEY speeddial_idx1 (uid, dial_did, dial_username),
    UNIQUE KEY speeddial_id (id),
    KEY speeddial_uid (uid)
);

CREATE TABLE sd_attrs (
    id VARCHAR(64) NOT NULL,
    name VARCHAR(32) NOT NULL,
    value VARCHAR(255),
    type INT NOT NULL DEFAULT '0',
    flags INT UNSIGNED NOT NULL DEFAULT '0',
    UNIQUE KEY userattrs_idx (id, name, value)
);

CREATE TABLE presentity (
    presid INT(10) UNSIGNED AUTO_INCREMENT NOT NULL,
    uri VARCHAR(255) NOT NULL,
    pdomain VARCHAR(128) NOT NULL,
    UNIQUE KEY presentity_key (presid),
    KEY presentity_key2 (uri)
);

CREATE TABLE presentity_contact (
    contactid INT(10) UNSIGNED AUTO_INCREMENT NOT NULL,
    presid INT(10) UNSIGNED NOT NULL,
    basic VARCHAR(32) NOT NULL DEFAULT 'offline',
    status VARCHAR(32) NOT NULL,
    location VARCHAR(128) NOT NULL,
    expires DATETIME NOT NULL DEFAULT '2020-05-28 21:32:15',
    placeid INT(10),
    priority FLOAT NOT NULL DEFAULT '0.5',
    contact VARCHAR(255),
    tupleid VARCHAR(64) NOT NULL,
    prescaps INT(10) NOT NULL,
    UNIQUE KEY pc_idx1 (contactid),
    KEY presid_index (presid),
    KEY location_index (location),
    KEY placeid_index (placeid)
);

CREATE TABLE watcherinfo (
    r_uri VARCHAR(255) NOT NULL,
    w_uri VARCHAR(255) NOT NULL,
    display_name VARCHAR(128) NOT NULL,
    s_id VARCHAR(64) NOT NULL,
    package VARCHAR(32) NOT NULL DEFAULT 'presence',
    status VARCHAR(32) NOT NULL DEFAULT 'pending',
    event VARCHAR(32) NOT NULL,
    expires INT NOT NULL,
    accepts INT NOT NULL,
    presid INT(10) UNSIGNED NOT NULL,
    server_contact VARCHAR(255) NOT NULL,
    dialog BLOB NOT NULL,
    doc_index INT NOT NULL,
    UNIQUE KEY wi_idx1 (s_id),
    KEY wi_ruri_idx (r_uri),
    KEY wi_wuri_idx (w_uri)
);

CREATE TABLE i18n (
    code INT NOT NULL,
    reason_re VARCHAR(255) DEFAULT NULL,
    lang VARCHAR(32) NOT NULL,
    new_reason VARCHAR(255),
    KEY i18n_idx (code)
);

INSERT INTO i18n (code, lang, new_reason) VALUES ('100', 'en_US.ascii', 'Trying');
INSERT INTO i18n (code, lang, new_reason) VALUES ('180', 'en_US.ascii', 'Ringing');
INSERT INTO i18n (code, lang, new_reason) VALUES ('181', 'en_US.ascii', 'Call Is Being Forwarded');
INSERT INTO i18n (code, lang, new_reason) VALUES ('182', 'en_US.ascii', 'Queued');
INSERT INTO i18n (code, lang, new_reason) VALUES ('183', 'en_US.ascii', 'Session Progress');
INSERT INTO i18n (code, lang, new_reason) VALUES ('200', 'en_US.ascii', 'OK');
INSERT INTO i18n (code, lang, new_reason) VALUES ('202', 'en_US.ascii', 'Pending');
INSERT INTO i18n (code, lang, new_reason) VALUES ('300', 'en_US.ascii', 'Multiple Choices');
INSERT INTO i18n (code, lang, new_reason) VALUES ('301', 'en_US.ascii', 'Moved Permanently');
INSERT INTO i18n (code, lang, new_reason) VALUES ('302', 'en_US.ascii', 'Moved Temporarily');
INSERT INTO i18n (code, lang, new_reason) VALUES ('305', 'en_US.ascii', 'Use Proxy');
INSERT INTO i18n (code, lang, new_reason) VALUES ('380', 'en_US.ascii', 'Alternative Service');
INSERT INTO i18n (code, lang, new_reason) VALUES ('400', 'en_US.ascii', 'Bad Request');
INSERT INTO i18n (code, lang, new_reason) VALUES ('401', 'en_US.ascii', 'Unauthorized');
INSERT INTO i18n (code, lang, new_reason) VALUES ('402', 'en_US.ascii', 'Payment Required');
INSERT INTO i18n (code, lang, new_reason) VALUES ('403', 'en_US.ascii', 'Forbidden');
INSERT INTO i18n (code, lang, new_reason) VALUES ('404', 'en_US.ascii', 'Not Found');
INSERT INTO i18n (code, lang, new_reason) VALUES ('405', 'en_US.ascii', 'Method Not Allowed');
INSERT INTO i18n (code, lang, new_reason) VALUES ('406', 'en_US.ascii', 'Not Acceptable');
INSERT INTO i18n (code, lang, new_reason) VALUES ('407', 'en_US.ascii', 'Proxy Authentication Required');
INSERT INTO i18n (code, lang, new_reason) VALUES ('408', 'en_US.ascii', 'Request Timeout');
INSERT INTO i18n (code, lang, new_reason) VALUES ('410', 'en_US.ascii', 'Gone');
INSERT INTO i18n (code, lang, new_reason) VALUES ('413', 'en_US.ascii', 'Request Entity Too Large');
INSERT INTO i18n (code, lang, new_reason) VALUES ('414', 'en_US.ascii', 'Request-URI Too Long');
INSERT INTO i18n (code, lang, new_reason) VALUES ('415', 'en_US.ascii', 'Unsupported Media Type');
INSERT INTO i18n (code, lang, new_reason) VALUES ('416', 'en_US.ascii', 'Unsupported URI Scheme');
INSERT INTO i18n (code, lang, new_reason) VALUES ('420', 'en_US.ascii', 'Bad Extension');
INSERT INTO i18n (code, lang, new_reason) VALUES ('421', 'en_US.ascii', 'Extension Required');
INSERT INTO i18n (code, lang, new_reason) VALUES ('423', 'en_US.ascii', 'Interval Too Brief');
INSERT INTO i18n (code, lang, new_reason) VALUES ('480', 'en_US.ascii', 'Temporarily Unavailable');
INSERT INTO i18n (code, lang, new_reason) VALUES ('481', 'en_US.ascii', 'Call/Transaction Does Not Exist');
INSERT INTO i18n (code, lang, new_reason) VALUES ('482', 'en_US.ascii', 'Loop Detected');
INSERT INTO i18n (code, lang, new_reason) VALUES ('483', 'en_US.ascii', 'Too Many Hops');
INSERT INTO i18n (code, lang, new_reason) VALUES ('484', 'en_US.ascii', 'Address Incomplete');
INSERT INTO i18n (code, lang, new_reason) VALUES ('485', 'en_US.ascii', 'Ambiguous');
INSERT INTO i18n (code, lang, new_reason) VALUES ('486', 'en_US.ascii', 'Busy Here');
INSERT INTO i18n (code, lang, new_reason) VALUES ('487', 'en_US.ascii', 'Request Terminated');
INSERT INTO i18n (code, lang, new_reason) VALUES ('488', 'en_US.ascii', 'Not Acceptable Here');
INSERT INTO i18n (code, lang, new_reason) VALUES ('491', 'en_US.ascii', 'Request Pending');
INSERT INTO i18n (code, lang, new_reason) VALUES ('493', 'en_US.ascii', 'Undecipherable');
INSERT INTO i18n (code, lang, new_reason) VALUES ('500', 'en_US.ascii', 'Server Internal Error');
INSERT INTO i18n (code, lang, new_reason) VALUES ('501', 'en_US.ascii', 'Not Implemented');
INSERT INTO i18n (code, lang, new_reason) VALUES ('502', 'en_US.ascii', 'Bad Gateway');
INSERT INTO i18n (code, lang, new_reason) VALUES ('503', 'en_US.ascii', 'Service Unavailable');
INSERT INTO i18n (code, lang, new_reason) VALUES ('504', 'en_US.ascii', 'Server Time-out');
INSERT INTO i18n (code, lang, new_reason) VALUES ('505', 'en_US.ascii', 'Version Not Supported');
INSERT INTO i18n (code, lang, new_reason) VALUES ('513', 'en_US.ascii', 'Message Too Large');
INSERT INTO i18n (code, lang, new_reason) VALUES ('600', 'en_US.ascii', 'Busy Everywhere');
INSERT INTO i18n (code, lang, new_reason) VALUES ('603', 'en_US.ascii', 'Decline');
INSERT INTO i18n (code, lang, new_reason) VALUES ('604', 'en_US.ascii', 'Does Not Exist Anywhere');
INSERT INTO i18n (code, lang, new_reason) VALUES ('606', 'en_US.ascii', 'Not Acceptable');
INSERT INTO i18n (code, lang, new_reason) VALUES ('100', 'cs_CZ.ascii', 'Navazuji spojeni');
INSERT INTO i18n (code, lang, new_reason) VALUES ('180', 'cs_CZ.ascii', 'Vyzvani');
INSERT INTO i18n (code, lang, new_reason) VALUES ('181', 'cs_CZ.ascii', 'Hovor je presmerovan');
INSERT INTO i18n (code, lang, new_reason) VALUES ('182', 'cs_CZ.ascii', 'Jste v poradi');
INSERT INTO i18n (code, lang, new_reason) VALUES ('183', 'cs_CZ.ascii', 'Probiha navazovani spojeni');
INSERT INTO i18n (code, lang, new_reason) VALUES ('200', 'cs_CZ.ascii', 'Uspesne provedeno');
INSERT INTO i18n (code, lang, new_reason) VALUES ('202', 'cs_CZ.ascii', 'Bude vyrizeno pozdeji');
INSERT INTO i18n (code, lang, new_reason) VALUES ('300', 'cs_CZ.ascii', 'Existuje vice moznosti');
INSERT INTO i18n (code, lang, new_reason) VALUES ('301', 'cs_CZ.ascii', 'Trvale presmerovano');
INSERT INTO i18n (code, lang, new_reason) VALUES ('302', 'cs_CZ.ascii', 'Docasne presmerovano');
INSERT INTO i18n (code, lang, new_reason) VALUES ('305', 'cs_CZ.ascii', 'Pouzijte jiny server');
INSERT INTO i18n (code, lang, new_reason) VALUES ('380', 'cs_CZ.ascii', 'Alternativni sluzba');
INSERT INTO i18n (code, lang, new_reason) VALUES ('400', 'cs_CZ.ascii', 'Chyba protokolu');
INSERT INTO i18n (code, lang, new_reason) VALUES ('401', 'cs_CZ.ascii', 'Overeni totoznosti');
INSERT INTO i18n (code, lang, new_reason) VALUES ('402', 'cs_CZ.ascii', 'Placena sluzba');
INSERT INTO i18n (code, lang, new_reason) VALUES ('403', 'cs_CZ.ascii', 'Zakazano');
INSERT INTO i18n (code, lang, new_reason) VALUES ('404', 'cs_CZ.ascii', 'Nenalezeno');
INSERT INTO i18n (code, lang, new_reason) VALUES ('405', 'cs_CZ.ascii', 'Nepovoleny prikaz');
INSERT INTO i18n (code, lang, new_reason) VALUES ('406', 'cs_CZ.ascii', 'Neni povoleno');
INSERT INTO i18n (code, lang, new_reason) VALUES ('407', 'cs_CZ.ascii', 'Server vyzaduje overeni totoznosti');
INSERT INTO i18n (code, lang, new_reason) VALUES ('408', 'cs_CZ.ascii', 'Casovy limit vyprsel');
INSERT INTO i18n (code, lang, new_reason) VALUES ('410', 'cs_CZ.ascii', 'Nenalezeno');
INSERT INTO i18n (code, lang, new_reason) VALUES ('413', 'cs_CZ.ascii', 'Prilis dlouhy identifikator');
INSERT INTO i18n (code, lang, new_reason) VALUES ('414', 'cs_CZ.ascii', 'Request-URI je prilis dlouhe');
INSERT INTO i18n (code, lang, new_reason) VALUES ('415', 'cs_CZ.ascii', 'Nepodporovany typ dat');
INSERT INTO i18n (code, lang, new_reason) VALUES ('416', 'cs_CZ.ascii', 'Nepodporovany typ identifikatoru');
INSERT INTO i18n (code, lang, new_reason) VALUES ('420', 'cs_CZ.ascii', 'Neplatne cislo linky');
INSERT INTO i18n (code, lang, new_reason) VALUES ('421', 'cs_CZ.ascii', 'Zadejte cislo linky');
INSERT INTO i18n (code, lang, new_reason) VALUES ('423', 'cs_CZ.ascii', 'Prilis kratky interval');
INSERT INTO i18n (code, lang, new_reason) VALUES ('480', 'cs_CZ.ascii', 'Docasne nedostupne');
INSERT INTO i18n (code, lang, new_reason) VALUES ('481', 'cs_CZ.ascii', 'Spojeni nenalezeno');
INSERT INTO i18n (code, lang, new_reason) VALUES ('482', 'cs_CZ.ascii', 'Zprava se zacyklila');
INSERT INTO i18n (code, lang, new_reason) VALUES ('483', 'cs_CZ.ascii', 'Prilis mnoho kroku');
INSERT INTO i18n (code, lang, new_reason) VALUES ('484', 'cs_CZ.ascii', 'Neuplna adresa');
INSERT INTO i18n (code, lang, new_reason) VALUES ('485', 'cs_CZ.ascii', 'Nejednoznacne');
INSERT INTO i18n (code, lang, new_reason) VALUES ('486', 'cs_CZ.ascii', 'Volany je zaneprazdnen');
INSERT INTO i18n (code, lang, new_reason) VALUES ('487', 'cs_CZ.ascii', 'Prikaz predcasne ukoncen');
INSERT INTO i18n (code, lang, new_reason) VALUES ('488', 'cs_CZ.ascii', 'Nebylo akceptovano');
INSERT INTO i18n (code, lang, new_reason) VALUES ('491', 'cs_CZ.ascii', 'Cekam na odpoved');
INSERT INTO i18n (code, lang, new_reason) VALUES ('493', 'cs_CZ.ascii', 'Nelze dekodovat');
INSERT INTO i18n (code, lang, new_reason) VALUES ('500', 'cs_CZ.ascii', 'Interni chyba serveru');
INSERT INTO i18n (code, lang, new_reason) VALUES ('501', 'cs_CZ.ascii', 'Neni implementovano');
INSERT INTO i18n (code, lang, new_reason) VALUES ('502', 'cs_CZ.ascii', 'Chybna brana');
INSERT INTO i18n (code, lang, new_reason) VALUES ('503', 'cs_CZ.ascii', 'Sluzba neni dostupna');
INSERT INTO i18n (code, lang, new_reason) VALUES ('504', 'cs_CZ.ascii', 'Casovy limit serveru vyprsel');
INSERT INTO i18n (code, lang, new_reason) VALUES ('505', 'cs_CZ.ascii', 'Nepodporovana verze protokolu');
INSERT INTO i18n (code, lang, new_reason) VALUES ('513', 'cs_CZ.ascii', 'Zprava je prilis dlouha');
INSERT INTO i18n (code, lang, new_reason) VALUES ('600', 'cs_CZ.ascii', 'Uzivatel je zaneprazdnen');
INSERT INTO i18n (code, lang, new_reason) VALUES ('603', 'cs_CZ.ascii', 'Odmitnuto');
INSERT INTO i18n (code, lang, new_reason) VALUES ('604', 'cs_CZ.ascii', 'Neexistujici uzivatel nebo sluzba');
INSERT INTO i18n (code, lang, new_reason) VALUES ('606', 'cs_CZ.ascii', 'Nelze akceptovat');

GRANT ALL ON ser.* TO 'ser'@'%' IDENTIFIED BY 'heslo';
GRANT ALL ON ser.* TO 'ser'@'localhost' IDENTIFIED BY 'heslo';
FLUSH PRIVILEGES;
GRANT SELECT ON ser.* TO 'serro'@'%' IDENTIFIED BY '47serro11';
GRANT SELECT ON ser.* TO 'serro'@'localhost' IDENTIFIED BY '47serro11';
FLUSH PRIVILEGES;
