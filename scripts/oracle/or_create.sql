CREATE TABLE version (
    table_name string(32) NOT NULL,
    table_version int NOT NULL DEFAULT '0'
);

INSERT INTO version (table_name, table_version) values ('acc','3');
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
    acc_id_key UNIQUE (id, ),

);

INSERT INTO version (table_name, table_version) values ('missed_calls','3');
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
    mc_id_key UNIQUE (id, ),

);

INSERT INTO version (table_name, table_version) values ('credentials','6');
CREATE TABLE credentials (
    auth_username string(64) NOT NULL,
    realm string(64) NOT NULL,
    password string(28) NOT NULL DEFAULT '',
    flags int NOT NULL DEFAULT '0',
    ha1 string(32) NOT NULL,
    ha1b string(32) NOT NULL DEFAULT '',
    uid string(64) NOT NULL
);

INSERT INTO version (table_name, table_version) values ('attr_types','2');
CREATE TABLE attr_types (
    name string(32) NOT NULL,
    rich_type string(32) NOT NULL DEFAULT 'string',
    raw_type int NOT NULL DEFAULT '2',
    type_spec string(255) DEFAULT NULL,
    description string(255) DEFAULT NULL,
    default_flags int NOT NULL DEFAULT '0',
    flags int NOT NULL DEFAULT '0',
    priority int NOT NULL DEFAULT '0',
    ordering int NOT NULL DEFAULT '0',
    upt_idx1 UNIQUE (name, )
);

INSERT INTO attr_types (name, raw_type, default_flags) VALUES ('uid', '2', '1');
INSERT INTO attr_types (name, raw_type, default_flags) VALUES ('did', '2', '1');
INSERT INTO attr_types (name, raw_type, rich_type, description, default_flags, priority, ordering) VALUES ('asserted_id', '2', 'string', 'asserted identity', '1', '0', '0');
INSERT INTO attr_types (name, raw_type, rich_type, description, default_flags, priority, ordering) VALUES ('fr_timer', '0', 'int', '@ff_fr_timer', '33', '1073807616', '140');
INSERT INTO attr_types (name, raw_type, rich_type, description, default_flags, priority, ordering) VALUES ('fr_inv_timer', '0', 'int', '@ff_fr_inv_timer', '33', '1073807616', '150');
INSERT INTO attr_types (name, raw_type, rich_type, description, default_flags, priority, ordering) VALUES ('gflags', '0', 'int', 'global flags', '33', '1073741824', '0');
INSERT INTO attr_types (name, raw_type, rich_type, description, default_flags, priority, ordering) VALUES ('digest_realm', '2', 'string', 'digest realm', '33', '65536', '0');
INSERT INTO attr_types (name, raw_type, rich_type, description, default_flags, flags, priority, ordering) VALUES ('acl', '2', 'string', 'access control list of user', '33', '1', '0', '0');
INSERT INTO attr_types (name, raw_type, rich_type, description, default_flags, flags, priority, ordering) VALUES ('first_name', '2', 'string', '@ff_first_name', '32', '2', '256', '10');
INSERT INTO attr_types (name, raw_type, rich_type, description, default_flags, flags, priority, ordering) VALUES ('last_name', '2', 'string', '@ff_last_name', '32', '2', '256', '20');
INSERT INTO attr_types (name, raw_type, rich_type, description, default_flags, flags, priority, ordering) VALUES ('email', '2', 'email_adr', '@ff_email', '33', '2', '256', '30');
INSERT INTO attr_types (name, raw_type, rich_type, description, default_flags, flags, priority, ordering) VALUES ('timezone', '2', 'timezone', '@ff_timezone', '32', '2', '1073807616', '60');
INSERT INTO attr_types (name, raw_type, rich_type, description, default_flags, flags, priority, ordering) VALUES ('sw_allow_find', '0', 'boolean', '@ff_allow_lookup_for_me', '32', '0', '256', '110');
INSERT INTO attr_types (name, raw_type, rich_type, description, default_flags, flags, priority, ordering) VALUES ('lang', '2', 'lang', '@ff_language', '33', '0', '1073807616', '50');
INSERT INTO attr_types (name, raw_type, rich_type, description, default_flags, flags, priority, ordering) VALUES ('sw_show_status', '0', 'boolean', '@ff_status_visibility', '32', '0', '1073742080', '100');
INSERT INTO attr_types (name, raw_type, rich_type, description, default_flags, flags, priority) VALUES ('sw_admin', '2', 'string', 'admin of domain', '32', '1', '0');
INSERT INTO attr_types (name, raw_type, rich_type, description, default_flags, flags, priority) VALUES ('sw_owner', '2', 'string', 'owner of domain', '32', '0', '0');
INSERT INTO attr_types (name, raw_type, rich_type, description, default_flags, flags, priority) VALUES ('sw_domain_default_flags', '0', 'int', '@ff_domain_def_f', '32', '0', '1073741824');
INSERT INTO attr_types (name, raw_type, rich_type, description, default_flags, flags, priority) VALUES ('sw_deleted_ts', '0', 'int', 'deleted timestamp', '32', '0', '0');
INSERT INTO attr_types (name, raw_type, rich_type, description, default_flags, flags, priority, ordering) VALUES ('phone', '2', 'string', '@ff_phone', '32', '2', '256', '40');
INSERT INTO attr_types (name, raw_type, rich_type, description, default_flags, flags, priority) VALUES ('sw_acl_control', '2', 'string', 'acl control', '32', '1', '0');
INSERT INTO attr_types (name, raw_type, rich_type, description, default_flags, flags, priority) VALUES ('sw_credential_default_flags', '0', 'int', '@ff_credential_def_f', '32', '0', '1073741824');
INSERT INTO attr_types (name, raw_type, rich_type, description, default_flags, flags, priority) VALUES ('sw_uri_default_flags', '0', 'int', '@ff_uri_def_f', '32', '0', '1073741824');
INSERT INTO attr_types (name, raw_type, rich_type, description, default_flags, flags, priority) VALUES ('sw_is_admin', '0', 'boolean', 'admin privilege', '32', '0', '0');
INSERT INTO attr_types (name, raw_type, rich_type, description, default_flags, flags, priority) VALUES ('sw_is_hostmaster', '0', 'boolean', 'hostmaster privilege', '32', '0', '0');
INSERT INTO attr_types (name, raw_type, rich_type, description, default_flags, flags, priority) VALUES ('sw_confirmation', '2', 'string', 'registration confirmation', '32', '0', '0');
INSERT INTO attr_types (name, raw_type, rich_type, description, default_flags, flags, priority) VALUES ('sw_pending_ts', '2', 'string', 'registration timestamp', '32', '0', '0');
INSERT INTO attr_types (name, raw_type, rich_type, description, default_flags, flags, priority, ordering) VALUES ('sw_require_confirm', '0', 'boolean', '@ff_reg_confirmation', '32', '0', '1073807360', '120');
INSERT INTO attr_types (name, raw_type, rich_type, description, default_flags, flags, priority, ordering) VALUES ('sw_send_missed', '0', 'boolean', '@ff_send_daily_missed_calls', '32', '0', '1073807616', '130');

INSERT INTO version (table_name, table_version) values ('global_attrs','1');
CREATE TABLE global_attrs (
    name string(32) NOT NULL,
    type int NOT NULL DEFAULT '0',
    value string(255),
    flags int NOT NULL DEFAULT '0',
    global_attrs_idx UNIQUE (name, value, )
);

INSERT INTO global_attrs (name, type, value, flags) VALUES ('sw_domain_default_flags', '0', '33', '32');
INSERT INTO global_attrs (name, type, value, flags) VALUES ('sw_credential_default_flags', '0', '33', '32');
INSERT INTO global_attrs (name, type, value, flags) VALUES ('sw_uri_default_flags', '0', '57', '32');
INSERT INTO global_attrs (name, type, value, flags) VALUES ('sw_show_status', '0', '1', '32');
INSERT INTO global_attrs (name, type, value, flags) VALUES ('sw_require_conf', '0', '1', '32');
INSERT INTO global_attrs (name, type, value, flags) VALUES ('lang', '2', 'en', '33');
INSERT INTO global_attrs (name, type, value, flags) VALUES ('sw_timezone', '2', 'Europe/Prague', '32');

INSERT INTO version (table_name, table_version) values ('domain_attrs','1');
CREATE TABLE domain_attrs (
    did string(64),
    name string(32) NOT NULL,
    type int NOT NULL DEFAULT '0',
    value string(255),
    flags int NOT NULL DEFAULT '0',
    domain_attr_idx UNIQUE (did, name, value, ),

);

INSERT INTO version (table_name, table_version) values ('user_attrs','3');
CREATE TABLE user_attrs (
    uid string(64) NOT NULL,
    name string(32) NOT NULL,
    value string(255),
    type int NOT NULL DEFAULT '0',
    flags int NOT NULL DEFAULT '0',
    userattrs_idx UNIQUE (uid, name, value, )
);

INSERT INTO version (table_name, table_version) values ('domain','2');
CREATE TABLE domain (
    did string(64) NOT NULL,
    domain string(128) NOT NULL,
    flags int NOT NULL DEFAULT '0',
    domain_idx UNIQUE (domain, )
);

INSERT INTO version (table_name, table_version) values ('domain_settings','1');
CREATE TABLE domain_settings (
    did string(64) NOT NULL,
    filename string(255) NOT NULL,
    version int NOT NULL,
    timestamp int,
    content binary,
    flags int NOT NULL DEFAULT '0',
    ds_id UNIQUE (did, filename, version, ),

);

INSERT INTO version (table_name, table_version) values ('location','8');
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
    instance string(255),
    location_key UNIQUE (uid, contact, ),

);

INSERT INTO version (table_name, table_version) values ('trusted','1');
CREATE TABLE trusted (
    src_ip string(39) NOT NULL,
    proto string(4) NOT NULL,
    from_pattern string(64) NOT NULL,
    trusted_idx UNIQUE (src_ip, proto, from_pattern, )
);

INSERT INTO version (table_name, table_version) values ('phonebook','1');
CREATE TABLE phonebook (
    id int NOT NULL,
    uid string(64) NOT NULL,
    fname string(32),
    lname string(32),
    sip_uri string(255) NOT NULL,
    pb_idx UNIQUE (id, ),

);

INSERT INTO version (table_name, table_version) values ('gw','3');
CREATE TABLE gw (
    gw_name string(128) NOT NULL,
    ip_addr int NOT NULL,
    port short,
    uri_scheme char,
    transport short,
    grp_id int NOT NULL,
    gw_idx1 UNIQUE (gw_name, ),

);

INSERT INTO version (table_name, table_version) values ('gw_grp','2');
CREATE TABLE gw_grp (
    grp_id int NOT NULL,
    grp_name string(64) NOT NULL,
    gwgrp_idx UNIQUE (grp_id, )
);

INSERT INTO version (table_name, table_version) values ('lcr','1');
CREATE TABLE lcr (
    prefix string(16) NOT NULL,
    from_uri string(255) NOT NULL DEFAULT '%',
    grp_id int,
    priority int
);

INSERT INTO version (table_name, table_version) values ('grp','3');
CREATE TABLE grp (
    uid string(64) NOT NULL DEFAULT '',
    grp string(64) NOT NULL DEFAULT '',
    last_modified datetime NOT NULL DEFAULT '1970-01-01 00:00:00'
);

INSERT INTO version (table_name, table_version) values ('silo','4');
CREATE TABLE silo (
    mid int NOT NULL,
    from_hdr string(255) NOT NULL,
    to_hdr string(255) NOT NULL,
    ruri string(255) NOT NULL,
    uid string(64) NOT NULL,
    inc_time datetime NOT NULL DEFAULT '1970-01-01 00:00:00',
    exp_time datetime NOT NULL DEFAULT '1970-01-01 00:00:00',
    ctype string(128) NOT NULL DEFAULT 'text/plain',
    body binary NOT NULL DEFAULT '',
    silo_idx1 UNIQUE (mid, )
);

INSERT INTO version (table_name, table_version) values ('uri','2');
CREATE TABLE uri (
    uid string(64) NOT NULL,
    did string(64) NOT NULL,
    username string(64) NOT NULL,
    flags int NOT NULL DEFAULT '0'
);

INSERT INTO version (table_name, table_version) values ('speed_dial','2');
CREATE TABLE speed_dial (
    id int NOT NULL,
    uid string(64) NOT NULL,
    dial_username string(64) NOT NULL,
    dial_did string(64) NOT NULL,
    new_uri string(255) NOT NULL,
    speeddial_idx1 UNIQUE (uid, dial_did, dial_username, ),
    speeddial_id UNIQUE (id, ),

);

INSERT INTO version (table_name, table_version) values ('sd_attrs','1');
CREATE TABLE sd_attrs (
    id string(64) NOT NULL,
    name string(32) NOT NULL,
    value string(255),
    type int NOT NULL DEFAULT '0',
    flags int NOT NULL DEFAULT '0',
    sd_idx UNIQUE (id, name, value, )
);

INSERT INTO version (table_name, table_version) values ('presentity','1');
CREATE TABLE presentity (
    presid int(10) NOT NULL,
    uri string(255) NOT NULL,
    uid string(64) NOT NULL,
    pdomain string(128) NOT NULL,
    presentity_key UNIQUE (presid, ),

);

INSERT INTO version (table_name, table_version) values ('presentity_notes','1');
CREATE TABLE presentity_notes (
    dbid string(64) NOT NULL,
    presid int(10) NOT NULL,
    etag string(64) NOT NULL,
    note string(128) NOT NULL,
    lang string(64) NOT NULL,
    expires datetime NOT NULL DEFAULT '2005-12-07 08:13:15',
    pnotes_idx1 UNIQUE (dbid, )
);

INSERT INTO version (table_name, table_version) values ('presentity_persons','1');
CREATE TABLE presentity_persons (
    dbid string(64) NOT NULL,
    presid int(10) NOT NULL,
    etag string(64) NOT NULL,
    person_element binary NOT NULL,
    id string(128) NOT NULL,
    expires datetime NOT NULL DEFAULT '2005-12-07 08:13:15',
    prespersons_idx1 UNIQUE (dbid, )
);

INSERT INTO version (table_name, table_version) values ('presentity_contact','1');
CREATE TABLE presentity_contact (
    contactid int(10) NOT NULL,
    presid int(10) NOT NULL,
    basic string(32) NOT NULL DEFAULT 'offline',
    status string(32) NOT NULL,
    location string(128) NOT NULL,
    expires datetime NOT NULL DEFAULT '2004-05-28 21:32:15',
    placeid int(10),
    priority float NOT NULL DEFAULT '0.5',
    contact string(255),
    tupleid string(64) NOT NULL,
    prescaps int(10) NOT NULL,
    etag string(64) NOT NULL,
    published_id string(64) NOT NULL,
    pc_idx1 UNIQUE (contactid, ),

);

INSERT INTO version (table_name, table_version) values ('watcherinfo','1');
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

INSERT INTO version (table_name, table_version) values ('tuple_notes','1');
CREATE TABLE tuple_notes (
    presid int(10) NOT NULL,
    tupleid string(64) NOT NULL,
    note string(128) NOT NULL,
    lang string(64) NOT NULL
);

INSERT INTO version (table_name, table_version) values ('offline_winfo','1');
CREATE TABLE offline_winfo (
    uid string(64) NOT NULL,
    watcher string(255) NOT NULL,
    events string(64) NOT NULL,
    domain string(128),
    status string(32),
    created_on datetime NOT NULL DEFAULT '2006-01-31 13:13:13',
    expires_on datetime NOT NULL DEFAULT '2006-01-31 13:13:13',
    dbid int(10) NOT NULL
);

INSERT INTO version (table_name, table_version) values ('rls_subscription','1');
CREATE TABLE rls_subscription (
    id string(48) NOT NULL,
    doc_version int NOT NULL,
    dialog binary NOT NULL,
    expires datetime NOT NULL DEFAULT '2005-12-02 09:00:13',
    status int NOT NULL,
    contact string(255) NOT NULL,
    uri string(255) NOT NULL,
    package string(128) NOT NULL,
    w_uri string(255) NOT NULL,
    xcap_root string(255) NOT NULL,
    rls_subscription_key UNIQUE (id, )
);

INSERT INTO version (table_name, table_version) values ('rls_vs','1');
CREATE TABLE rls_vs (
    id string(48) NOT NULL,
    rls_id string(48) NOT NULL,
    uri string(255) NOT NULL,
    rls_vs_key UNIQUE (id, )
);

INSERT INTO version (table_name, table_version) values ('rls_vs_names','1');
CREATE TABLE rls_vs_names (
    id string(48) NOT NULL,
    name string(64),
    lang string(64)
);

INSERT INTO version (table_name, table_version) values ('i18n','1');
CREATE TABLE i18n (
    code int NOT NULL,
    reason_re string(255) DEFAULT NULL,
    lang string(32) NOT NULL,
    new_reason string(255)
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

INSERT INTO version (table_name, table_version) values ('pdt','1');
CREATE TABLE pdt (
    prefix string(32) NOT NULL,
    domain string(255) NOT NULL,
    pdt_idx UNIQUE (prefix, )
);

INSERT INTO version (table_name, table_version) values ('cpl','2');
CREATE TABLE cpl (
    uid string(64) NOT NULL,
    cpl_xml binary,
    cpl_bin binary,
    cpl_key UNIQUE (uid, )
);

INSERT INTO version (table_name, table_version) values ('customers','1');
CREATE TABLE customers (
    cid int NOT NULL,
    name string(128) NOT NULL,
    address string(255),
    phone string(64),
    email string(255),
    cu_idx UNIQUE (cid, )
);

 