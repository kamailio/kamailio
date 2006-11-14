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

INSERT INTO version (table_name, table_version) values ('credentials','7');
CREATE TABLE credentials (
    auth_username string(64) NOT NULL,
    did string(64) NOT NULL DEFAULT '_none',
    realm string(64) NOT NULL,
    password string(28) NOT NULL DEFAULT '',
    flags int NOT NULL DEFAULT '0',
    ha1 string(32) NOT NULL,
    ha1b string(32) NOT NULL DEFAULT '',
    uid string(64) NOT NULL
);

INSERT INTO version (table_name, table_version) values ('attr_types','3');
CREATE TABLE attr_types (
    name string(32) NOT NULL,
    rich_type string(32) NOT NULL DEFAULT 'string',
    raw_type int NOT NULL DEFAULT '2',
    type_spec string(255) DEFAULT NULL,
    description string(255) DEFAULT NULL,
    default_flags int NOT NULL DEFAULT '0',
    flags int NOT NULL DEFAULT '0',
    priority int NOT NULL DEFAULT '0',
    access int NOT NULL DEFAULT '0',
    ordering int NOT NULL DEFAULT '0',
    upt_idx1 UNIQUE (name, )
);

INSERT INTO version (table_name, table_version) values ('global_attrs','1');
CREATE TABLE global_attrs (
    name string(32) NOT NULL,
    type int NOT NULL DEFAULT '0',
    value string(255),
    flags int NOT NULL DEFAULT '0',
    global_attrs_idx UNIQUE (name, value, )
);

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

INSERT INTO version (table_name, table_version) values ('uri_attrs','1');
CREATE TABLE uri_attrs (
    username string(64) NOT NULL,
    did string(64) NOT NULL,
    name string(32) NOT NULL,
    value string(255),
    type int NOT NULL DEFAULT '0',
    flags int NOT NULL DEFAULT '0',
    scheme int NOT NULL DEFAULT '0',
    uriattrs_idx UNIQUE (username, did, name, value, )
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

INSERT INTO version (table_name, table_version) values ('location','9');
CREATE TABLE location (
    uid string(64) NOT NULL,
    aor string(255) NOT NULL,
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

INSERT INTO version (table_name, table_version) values ('contact_attrs','1');
CREATE TABLE contact_attrs (
    uid string(64) NOT NULL,
    contact string(255) NOT NULL,
    name string(32) NOT NULL,
    value string(255),
    type int NOT NULL DEFAULT '0',
    flags int NOT NULL DEFAULT '0',
    contactattrs_idx UNIQUE (uid, contact, name, )
);

INSERT INTO version (table_name, table_version) values ('trusted','1');
CREATE TABLE trusted (
    src_ip string(39) NOT NULL,
    proto string(4) NOT NULL,
    from_pattern string(64) NOT NULL,
    trusted_idx UNIQUE (src_ip, proto, from_pattern, )
);

INSERT INTO version (table_name, table_version) values ('ipmatch','1');
CREATE TABLE ipmatch (
    ip string(50) NOT NULL DEFAULT '',
    avp_val string(30) DEFAULT NULL,
    mark int(10) NOT NULL DEFAULT '1',
    flags int(10) NOT NULL DEFAULT '0',
    ipmatch_idx UNIQUE (ip, mark, )
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

INSERT INTO version (table_name, table_version) values ('uri','3');
CREATE TABLE uri (
    uid string(64) NOT NULL,
    did string(64) NOT NULL,
    username string(64) NOT NULL,
    flags int NOT NULL DEFAULT '0',
    scheme string(8) NOT NULL DEFAULT 'sip'
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

INSERT INTO version (table_name, table_version) values ('presentity','5');
CREATE TABLE presentity (
    pres_id string(64) NOT NULL,
    uri string(255) NOT NULL,
    uid string(64) NOT NULL,
    pdomain string(128) NOT NULL,
    xcap_params binary NOT NULL,
    presentity_key UNIQUE (pres_id, )
);

INSERT INTO version (table_name, table_version) values ('presentity_notes','5');
CREATE TABLE presentity_notes (
    dbid string(64) NOT NULL,
    pres_id string(64) NOT NULL,
    etag string(64) NOT NULL,
    note string(128) NOT NULL,
    lang string(64) NOT NULL,
    expires datetime NOT NULL DEFAULT '2005-12-07 08:13:15',
    pnotes_idx1 UNIQUE (dbid, )
);

INSERT INTO version (table_name, table_version) values ('presentity_extensions','5');
CREATE TABLE presentity_extensions (
    dbid string(64) NOT NULL,
    pres_id string(64) NOT NULL,
    etag string(64) NOT NULL,
    element binary NOT NULL,
    expires datetime NOT NULL DEFAULT '2005-12-07 08:13:15',
    presextensions_idx1 UNIQUE (dbid, )
);

INSERT INTO version (table_name, table_version) values ('presentity_contact','5');
CREATE TABLE presentity_contact (
    pres_id string(64) NOT NULL,
    basic int(3) NOT NULL,
    expires datetime NOT NULL DEFAULT '2004-05-28 21:32:15',
    priority float NOT NULL DEFAULT '0.5',
    contact string(255),
    tupleid string(64) NOT NULL,
    etag string(64) NOT NULL,
    published_id string(64) NOT NULL,
    presid_index UNIQUE (pres_id, tupleid, )
);

INSERT INTO version (table_name, table_version) values ('watcherinfo','5');
CREATE TABLE watcherinfo (
    w_uri string(255) NOT NULL,
    display_name string(128) NOT NULL,
    s_id string(64) NOT NULL,
    package string(32) NOT NULL DEFAULT 'presence',
    status string(32) NOT NULL DEFAULT 'pending',
    event string(32) NOT NULL,
    expires datetime NOT NULL DEFAULT '2005-12-07 08:13:15',
    accepts int NOT NULL,
    pres_id string(64) NOT NULL,
    server_contact string(255) NOT NULL,
    dialog binary NOT NULL,
    doc_index int NOT NULL,
    wi_idx1 UNIQUE (s_id, )
);

INSERT INTO version (table_name, table_version) values ('tuple_notes','5');
CREATE TABLE tuple_notes (
    pres_id string(64) NOT NULL,
    tupleid string(64) NOT NULL,
    note string(128) NOT NULL,
    lang string(64) NOT NULL
);

INSERT INTO version (table_name, table_version) values ('tuple_extensions','5');
CREATE TABLE tuple_extensions (
    pres_id string(64) NOT NULL,
    tupleid string(64) NOT NULL,
    element binary NOT NULL,
    status_extension int(1) NOT NULL
);

INSERT INTO version (table_name, table_version) values ('offline_winfo','5');
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
    xcap_params binary NOT NULL,
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
    new_reason string(255)    i18n_uniq_idx UNIQUE (code, lang, )
);

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

 