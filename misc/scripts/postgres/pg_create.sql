
CREATE TABLE acc (
    id SERIAL NOT NULL,
    server_id INTEGER NOT NULL DEFAULT '0',
    from_uid VARCHAR(64),
    to_uid VARCHAR(64),
    to_did VARCHAR(64),
    from_did VARCHAR(64),
    sip_from VARCHAR(255),
    sip_to VARCHAR(255),
    sip_status INTEGER,
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
    CONSTRAINT acc_id_key UNIQUE (server_id, id)
);

CREATE INDEX acc_cid_key ON acc (sip_callid);
CREATE INDEX acc_from_uid ON acc (from_uid);
CREATE INDEX acc_to_uid ON acc (to_uid);

CREATE TABLE missed_calls (
    id SERIAL NOT NULL,
    server_id INTEGER NOT NULL DEFAULT '0',
    from_uid VARCHAR(64),
    to_uid VARCHAR(64),
    to_did VARCHAR(64),
    from_did VARCHAR(64),
    sip_from VARCHAR(255),
    sip_to VARCHAR(255),
    sip_status INTEGER,
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
    CONSTRAINT mc_id_key UNIQUE (server_id, id)
);

CREATE INDEX mc_cid_key ON missed_calls (sip_callid);
CREATE INDEX mc_to_uid ON missed_calls (to_uid);

CREATE TABLE credentials (
    auth_username VARCHAR(64) NOT NULL,
    did VARCHAR(64) NOT NULL DEFAULT '_default',
    realm VARCHAR(64) NOT NULL,
    password VARCHAR(28) NOT NULL DEFAULT '',
    flags INTEGER NOT NULL DEFAULT '0',
    ha1 VARCHAR(32) NOT NULL,
    ha1b VARCHAR(32) NOT NULL DEFAULT '',
    uid VARCHAR(64) NOT NULL
);

CREATE INDEX cred_idx ON credentials (auth_username, did);
CREATE INDEX uid ON credentials (uid);
CREATE INDEX did_idx ON credentials (did);
CREATE INDEX realm_idx ON credentials (realm);

CREATE TABLE attr_types (
    name VARCHAR(32) NOT NULL,
    rich_type VARCHAR(32) NOT NULL DEFAULT 'string',
    raw_type INTEGER NOT NULL DEFAULT '2',
    type_spec VARCHAR(255) DEFAULT NULL,
    description VARCHAR(255) DEFAULT NULL,
    default_flags INTEGER NOT NULL DEFAULT '0',
    flags INTEGER NOT NULL DEFAULT '0',
    priority INTEGER NOT NULL DEFAULT '0',
    access INTEGER NOT NULL DEFAULT '0',
    ordering INTEGER NOT NULL DEFAULT '0',
    grp VARCHAR(32) NOT NULL DEFAULT 'other',
    CONSTRAINT upt_idx1 UNIQUE (name)
);

CREATE TABLE global_attrs (
    name VARCHAR(32) NOT NULL,
    type INTEGER NOT NULL DEFAULT '0',
    value VARCHAR(255),
    flags INTEGER NOT NULL DEFAULT '0',
    CONSTRAINT global_attrs_idx UNIQUE (name, value)
);

CREATE TABLE domain_attrs (
    did VARCHAR(64),
    name VARCHAR(32) NOT NULL,
    type INTEGER NOT NULL DEFAULT '0',
    value VARCHAR(255),
    flags INTEGER NOT NULL DEFAULT '0',
    CONSTRAINT domain_attr_idx UNIQUE (did, name, value)
);

CREATE INDEX domain_did ON domain_attrs (did, flags);

CREATE TABLE user_attrs (
    uid VARCHAR(64) NOT NULL,
    name VARCHAR(32) NOT NULL,
    value VARCHAR(255),
    type INTEGER NOT NULL DEFAULT '0',
    flags INTEGER NOT NULL DEFAULT '0',
    CONSTRAINT userattrs_idx UNIQUE (uid, name, value)
);

CREATE TABLE uri_attrs (
    username VARCHAR(64) NOT NULL,
    did VARCHAR(64) NOT NULL,
    name VARCHAR(32) NOT NULL,
    value VARCHAR(255),
    type INTEGER NOT NULL DEFAULT '0',
    flags INTEGER NOT NULL DEFAULT '0',
    scheme VARCHAR(8) NOT NULL DEFAULT 'sip',
    CONSTRAINT uriattrs_idx UNIQUE (username, did, name, value, scheme)
);

CREATE TABLE domain (
    did VARCHAR(64) NOT NULL,
    domain VARCHAR(128) NOT NULL,
    flags INTEGER NOT NULL DEFAULT '0',
    CONSTRAINT domain_idx UNIQUE (domain)
);

CREATE INDEX did_idx ON domain (did);

CREATE TABLE domain_settings (
    did VARCHAR(64) NOT NULL,
    filename VARCHAR(255) NOT NULL,
    version INTEGER NOT NULL,
    timestamp INTEGER,
    content BYTEA,
    flags INTEGER NOT NULL DEFAULT '0',
    CONSTRAINT ds_id UNIQUE (did, filename, version)
);

CREATE INDEX ds_df ON domain_settings (did, filename);

CREATE TABLE location (
    uid VARCHAR(64) NOT NULL,
    aor VARCHAR(255) NOT NULL,
    contact VARCHAR(255) NOT NULL,
    server_id INTEGER NOT NULL DEFAULT '0',
    received VARCHAR(255),
    expires TIMESTAMP NOT NULL DEFAULT '1970-01-01 00:00:00',
    q REAL NOT NULL DEFAULT '1.0',
    callid VARCHAR(255),
    cseq INTEGER,
    flags INTEGER NOT NULL DEFAULT '0',
    user_agent VARCHAR(64),
    instance VARCHAR(255),
    CONSTRAINT location_key UNIQUE (uid, contact)
);

CREATE INDEX location_contact ON location (contact);
CREATE INDEX location_expires ON location (expires);

CREATE TABLE contact_attrs (
    uid VARCHAR(64) NOT NULL,
    contact VARCHAR(255) NOT NULL,
    name VARCHAR(32) NOT NULL,
    value VARCHAR(255),
    type INTEGER NOT NULL DEFAULT '0',
    flags INTEGER NOT NULL DEFAULT '0',
    CONSTRAINT contactattrs_idx UNIQUE (uid, contact, name)
);

CREATE TABLE trusted (
    src_ip VARCHAR(39) NOT NULL,
    proto VARCHAR(4) NOT NULL,
    from_pattern VARCHAR(64) NOT NULL,
    CONSTRAINT trusted_idx UNIQUE (src_ip, proto, from_pattern)
);

CREATE TABLE ipmatch (
    ip VARCHAR(50) NOT NULL DEFAULT '',
    avp_val VARCHAR(30) DEFAULT NULL,
    mark INTEGER NOT NULL DEFAULT '1',
    flags INTEGER NOT NULL DEFAULT '0',
    CONSTRAINT ipmatch_idx UNIQUE (ip, mark)
);

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
    prefix VARCHAR(16) NOT NULL,
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

CREATE TABLE grp (
    uid VARCHAR(64) NOT NULL DEFAULT '',
    grp VARCHAR(64) NOT NULL DEFAULT '',
    last_modified TIMESTAMP NOT NULL DEFAULT '1970-01-01 00:00:00'
);

CREATE INDEX grp_idx ON grp (uid, grp);

CREATE TABLE silo (
    mid SERIAL NOT NULL,
    from_hdr VARCHAR(255) NOT NULL,
    to_hdr VARCHAR(255) NOT NULL,
    ruri VARCHAR(255) NOT NULL,
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
    scheme VARCHAR(8) NOT NULL DEFAULT 'sip'
);

CREATE INDEX uri_idx1 ON uri (username, did, scheme);
CREATE INDEX uri_uid ON uri (uid);

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
    value VARCHAR(255),
    type INTEGER NOT NULL DEFAULT '0',
    flags INTEGER NOT NULL DEFAULT '0',
    CONSTRAINT sd_idx UNIQUE (id, name, value)
);

CREATE TABLE presentity (
    pres_id VARCHAR(64) NOT NULL,
    uri VARCHAR(255) NOT NULL,
    uid VARCHAR(64) NOT NULL,
    pdomain VARCHAR(128) NOT NULL,
    xcap_params BYTEA NOT NULL,
    CONSTRAINT presentity_key UNIQUE (pres_id)
);

CREATE TABLE presentity_notes (
    dbid VARCHAR(64) NOT NULL,
    pres_id VARCHAR(64) NOT NULL,
    etag VARCHAR(64) NOT NULL,
    note VARCHAR(128) NOT NULL,
    lang VARCHAR(64) NOT NULL,
    expires TIMESTAMP NOT NULL DEFAULT '2005-12-07 08:13:15',
    CONSTRAINT pnotes_idx1 UNIQUE (dbid)
);

CREATE TABLE presentity_extensions (
    dbid VARCHAR(64) NOT NULL,
    pres_id VARCHAR(64) NOT NULL,
    etag VARCHAR(64) NOT NULL,
    element BYTEA NOT NULL,
    expires TIMESTAMP NOT NULL DEFAULT '2005-12-07 08:13:15',
    CONSTRAINT presextensions_idx1 UNIQUE (dbid)
);

CREATE TABLE presentity_contact (
    pres_id VARCHAR(64) NOT NULL,
    basic INTEGER NOT NULL,
    expires TIMESTAMP NOT NULL DEFAULT '2004-05-28 21:32:15',
    priority REAL NOT NULL DEFAULT '0.5',
    contact VARCHAR(255),
    tupleid VARCHAR(64) NOT NULL,
    etag VARCHAR(64) NOT NULL,
    published_id VARCHAR(64) NOT NULL,
    CONSTRAINT presid_index UNIQUE (pres_id, tupleid)
);

CREATE TABLE watcherinfo (
    w_uri VARCHAR(255) NOT NULL,
    display_name VARCHAR(128) NOT NULL,
    s_id VARCHAR(64) NOT NULL,
    package VARCHAR(32) NOT NULL DEFAULT 'presence',
    status VARCHAR(32) NOT NULL DEFAULT 'pending',
    event VARCHAR(32) NOT NULL,
    expires TIMESTAMP NOT NULL DEFAULT '2005-12-07 08:13:15',
    accepts INTEGER NOT NULL,
    pres_id VARCHAR(64) NOT NULL,
    server_contact VARCHAR(255) NOT NULL,
    dialog BYTEA NOT NULL,
    doc_index INTEGER NOT NULL,
    CONSTRAINT wi_idx1 UNIQUE (s_id)
);

CREATE TABLE tuple_notes (
    pres_id VARCHAR(64) NOT NULL,
    tupleid VARCHAR(64) NOT NULL,
    note VARCHAR(128) NOT NULL,
    lang VARCHAR(64) NOT NULL
);

CREATE TABLE tuple_extensions (
    pres_id VARCHAR(64) NOT NULL,
    tupleid VARCHAR(64) NOT NULL,
    element BYTEA NOT NULL,
    status_extension INTEGER NOT NULL
);

CREATE TABLE offline_winfo (
    uid VARCHAR(64) NOT NULL,
    watcher VARCHAR(255) NOT NULL,
    events VARCHAR(64) NOT NULL,
    domain VARCHAR(128),
    status VARCHAR(32),
    created_on TIMESTAMP NOT NULL DEFAULT '2006-01-31 13:13:13',
    expires_on TIMESTAMP NOT NULL DEFAULT '2006-01-31 13:13:13',
    dbid SERIAL NOT NULL,
    CONSTRAINT offline_winfo_key UNIQUE (dbid)
);

CREATE TABLE rls_subscription (
    id VARCHAR(48) NOT NULL,
    doc_version INTEGER NOT NULL,
    dialog BYTEA NOT NULL,
    expires TIMESTAMP NOT NULL DEFAULT '2005-12-02 09:00:13',
    status INTEGER NOT NULL,
    contact VARCHAR(255) NOT NULL,
    uri VARCHAR(255) NOT NULL,
    package VARCHAR(128) NOT NULL,
    w_uri VARCHAR(255) NOT NULL,
    xcap_params BYTEA NOT NULL,
    CONSTRAINT rls_subscription_key UNIQUE (id)
);

CREATE TABLE rls_vs (
    id VARCHAR(48) NOT NULL,
    rls_id VARCHAR(48) NOT NULL,
    uri VARCHAR(255) NOT NULL,
    CONSTRAINT rls_vs_key UNIQUE (id)
);

CREATE TABLE rls_vs_names (
    id VARCHAR(48) NOT NULL,
    name VARCHAR(64),
    lang VARCHAR(64)
);

CREATE TABLE i18n (
    code INTEGER NOT NULL,
    reason_re VARCHAR(255) DEFAULT NULL,
    lang VARCHAR(32) NOT NULL,
    new_reason VARCHAR(255),
    CONSTRAINT i18n_uniq_idx UNIQUE (code, lang)
);

CREATE INDEX i18n_idx ON i18n (code);

CREATE TABLE pdt (
    prefix VARCHAR(32) NOT NULL,
    domain VARCHAR(255) NOT NULL,
    CONSTRAINT pdt_idx UNIQUE (prefix)
);

CREATE TABLE cpl (
    uid VARCHAR(64) NOT NULL,
    cpl_xml BYTEA,
    cpl_bin BYTEA,
    CONSTRAINT cpl_key UNIQUE (uid)
);

CREATE TABLE customers (
    cid SERIAL NOT NULL,
    name VARCHAR(128) NOT NULL,
    address VARCHAR(255),
    phone VARCHAR(64),
    email VARCHAR(255),
    CONSTRAINT cu_idx UNIQUE (cid)
);


 