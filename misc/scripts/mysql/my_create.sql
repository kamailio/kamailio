CREATE TABLE acc (
    id INT AUTO_INCREMENT NOT NULL,
    server_id INT NOT NULL DEFAULT '0',
    from_uid VARCHAR(64),
    to_uid VARCHAR(64),
    to_did VARCHAR(64),
    from_did VARCHAR(64),
    sip_from VARCHAR(255),
    sip_to VARCHAR(255),
    sip_status INT,
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
    UNIQUE KEY acc_id_key (server_id, id),
    KEY acc_cid_key (sip_callid),
    KEY acc_from_uid (from_uid),
    KEY acc_to_uid (to_uid)
);

CREATE TABLE missed_calls (
    id INT AUTO_INCREMENT NOT NULL,
    server_id INT NOT NULL DEFAULT '0',
    from_uid VARCHAR(64),
    to_uid VARCHAR(64),
    to_did VARCHAR(64),
    from_did VARCHAR(64),
    sip_from VARCHAR(255),
    sip_to VARCHAR(255),
    sip_status INT,
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
    UNIQUE KEY mc_id_key (server_id, id),
    KEY mc_cid_key (sip_callid),
    KEY mc_to_uid (to_uid)
);

CREATE TABLE credentials (
    auth_username VARCHAR(64) NOT NULL,
    did VARCHAR(64) NOT NULL DEFAULT '_default',
    realm VARCHAR(64) NOT NULL,
    password VARCHAR(28) NOT NULL DEFAULT '',
    flags INT NOT NULL DEFAULT '0',
    ha1 VARCHAR(32) NOT NULL,
    ha1b VARCHAR(32) NOT NULL DEFAULT '',
    uid VARCHAR(64) NOT NULL,
    KEY cred_idx (auth_username, did),
    KEY uid (uid),
    KEY did_idx (did),
    KEY realm_idx (realm)
);

CREATE TABLE attr_types (
    name VARCHAR(32) NOT NULL,
    rich_type VARCHAR(32) NOT NULL DEFAULT 'string',
    raw_type INT NOT NULL DEFAULT '2',
    type_spec VARCHAR(255) DEFAULT NULL,
    description VARCHAR(255) DEFAULT NULL,
    default_flags INT NOT NULL DEFAULT '0',
    flags INT NOT NULL DEFAULT '0',
    priority INT NOT NULL DEFAULT '0',
    access INT NOT NULL DEFAULT '0',
    ordering INT NOT NULL DEFAULT '0',
    grp VARCHAR(32) NOT NULL DEFAULT 'other',
    UNIQUE KEY upt_idx1 (name)
);

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

CREATE TABLE uri_attrs (
    username VARCHAR(64) NOT NULL,
    did VARCHAR(64) NOT NULL,
    name VARCHAR(32) NOT NULL,
    value VARCHAR(255),
    type INT NOT NULL DEFAULT '0',
    flags INT UNSIGNED NOT NULL DEFAULT '0',
    scheme VARCHAR(8) NOT NULL DEFAULT 'sip',
    UNIQUE KEY uriattrs_idx (username, did, name, value, scheme)
);

CREATE TABLE domain (
    did VARCHAR(64) NOT NULL,
    domain VARCHAR(128) NOT NULL,
    flags INT UNSIGNED NOT NULL DEFAULT '0',
    UNIQUE KEY domain_idx (domain),
    KEY did_idx (did)
);

CREATE TABLE domain_settings (
    did VARCHAR(64) NOT NULL,
    filename VARCHAR(255) NOT NULL,
    version INT UNSIGNED NOT NULL,
    timestamp INT UNSIGNED,
    content BLOB,
    flags INT UNSIGNED NOT NULL DEFAULT '0',
    UNIQUE KEY ds_id (did, filename, version),
    KEY ds_df (did, filename)
);

CREATE TABLE location (
    uid VARCHAR(64) NOT NULL,
    aor VARCHAR(255) NOT NULL,
    contact VARCHAR(255) NOT NULL,
    server_id INT NOT NULL DEFAULT '0',
    received VARCHAR(255),
    expires DATETIME NOT NULL DEFAULT '1970-01-01 00:00:00',
    q FLOAT NOT NULL DEFAULT '1.0',
    callid VARCHAR(255),
    cseq INT UNSIGNED,
    flags INT UNSIGNED NOT NULL DEFAULT '0',
    user_agent VARCHAR(64),
    instance VARCHAR(255),
    UNIQUE KEY location_key (uid, contact),
    KEY location_contact (contact),
    KEY location_expires (expires)
);

CREATE TABLE contact_attrs (
    uid VARCHAR(64) NOT NULL,
    contact VARCHAR(255) NOT NULL,
    name VARCHAR(32) NOT NULL,
    value VARCHAR(255),
    type INT NOT NULL DEFAULT '0',
    flags INT UNSIGNED NOT NULL DEFAULT '0',
    UNIQUE KEY contactattrs_idx (uid, contact, name)
);

CREATE TABLE trusted (
    src_ip VARCHAR(39) NOT NULL,
    proto VARCHAR(4) NOT NULL,
    from_pattern VARCHAR(64) NOT NULL,
    UNIQUE KEY trusted_idx (src_ip, proto, from_pattern)
);

CREATE TABLE ipmatch (
    ip VARCHAR(50) NOT NULL DEFAULT '',
    avp_val VARCHAR(30) DEFAULT NULL,
    mark INT(10) UNSIGNED NOT NULL DEFAULT '1',
    flags INT(10) UNSIGNED NOT NULL DEFAULT '0',
    UNIQUE KEY ipmatch_idx (ip, mark)
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
    prefix VARCHAR(16) NOT NULL,
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
    from_hdr VARCHAR(255) NOT NULL,
    to_hdr VARCHAR(255) NOT NULL,
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
    scheme VARCHAR(8) NOT NULL DEFAULT 'sip',
    KEY uri_idx1 (username, did, scheme),
    KEY uri_uid (uid)
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
    UNIQUE KEY sd_idx (id, name, value)
);

CREATE TABLE presentity (
    pres_id VARCHAR(64) NOT NULL,
    uri VARCHAR(255) NOT NULL,
    uid VARCHAR(64) NOT NULL,
    pdomain VARCHAR(128) NOT NULL,
    xcap_params BLOB NOT NULL,
    UNIQUE KEY presentity_key (pres_id)
);

CREATE TABLE presentity_notes (
    dbid VARCHAR(64) NOT NULL,
    pres_id VARCHAR(64) NOT NULL,
    etag VARCHAR(64) NOT NULL,
    note VARCHAR(128) NOT NULL,
    lang VARCHAR(64) NOT NULL,
    expires DATETIME NOT NULL DEFAULT '2005-12-07 08:13:15',
    UNIQUE KEY pnotes_idx1 (dbid)
);

CREATE TABLE presentity_extensions (
    dbid VARCHAR(64) NOT NULL,
    pres_id VARCHAR(64) NOT NULL,
    etag VARCHAR(64) NOT NULL,
    element BLOB NOT NULL,
    expires DATETIME NOT NULL DEFAULT '2005-12-07 08:13:15',
    UNIQUE KEY presextensions_idx1 (dbid)
);

CREATE TABLE presentity_contact (
    pres_id VARCHAR(64) NOT NULL,
    basic INT(3) NOT NULL,
    expires DATETIME NOT NULL DEFAULT '2004-05-28 21:32:15',
    priority FLOAT NOT NULL DEFAULT '0.5',
    contact VARCHAR(255),
    tupleid VARCHAR(64) NOT NULL,
    etag VARCHAR(64) NOT NULL,
    published_id VARCHAR(64) NOT NULL,
    UNIQUE KEY presid_index (pres_id, tupleid)
);

CREATE TABLE watcherinfo (
    w_uri VARCHAR(255) NOT NULL,
    display_name VARCHAR(128) NOT NULL,
    s_id VARCHAR(64) NOT NULL,
    package VARCHAR(32) NOT NULL DEFAULT 'presence',
    status VARCHAR(32) NOT NULL DEFAULT 'pending',
    event VARCHAR(32) NOT NULL,
    expires DATETIME NOT NULL DEFAULT '2005-12-07 08:13:15',
    accepts INT NOT NULL,
    pres_id VARCHAR(64) NOT NULL,
    server_contact VARCHAR(255) NOT NULL,
    dialog BLOB NOT NULL,
    doc_index INT NOT NULL,
    UNIQUE KEY wi_idx1 (s_id)
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
    element BLOB NOT NULL,
    status_extension INT(1) NOT NULL
);

CREATE TABLE offline_winfo (
    uid VARCHAR(64) NOT NULL,
    watcher VARCHAR(255) NOT NULL,
    events VARCHAR(64) NOT NULL,
    domain VARCHAR(128),
    status VARCHAR(32),
    created_on DATETIME NOT NULL DEFAULT '2006-01-31 13:13:13',
    expires_on DATETIME NOT NULL DEFAULT '2006-01-31 13:13:13',
    dbid INT(10) UNSIGNED AUTO_INCREMENT NOT NULL,
    UNIQUE KEY offline_winfo_key (dbid)
);

CREATE TABLE rls_subscription (
    id VARCHAR(48) NOT NULL,
    doc_version INT NOT NULL,
    dialog BLOB NOT NULL,
    expires DATETIME NOT NULL DEFAULT '2005-12-02 09:00:13',
    status INT NOT NULL,
    contact VARCHAR(255) NOT NULL,
    uri VARCHAR(255) NOT NULL,
    package VARCHAR(128) NOT NULL,
    w_uri VARCHAR(255) NOT NULL,
    xcap_params BLOB NOT NULL,
    UNIQUE KEY rls_subscription_key (id)
);

CREATE TABLE rls_vs (
    id VARCHAR(48) NOT NULL,
    rls_id VARCHAR(48) NOT NULL,
    uri VARCHAR(255) NOT NULL,
    UNIQUE KEY rls_vs_key (id)
);

CREATE TABLE rls_vs_names (
    id VARCHAR(48) NOT NULL,
    name VARCHAR(64),
    lang VARCHAR(64)
);

CREATE TABLE i18n (
    code INT NOT NULL,
    reason_re VARCHAR(255) DEFAULT NULL,
    lang VARCHAR(32) NOT NULL,
    new_reason VARCHAR(255),
    KEY i18n_idx (code),
    UNIQUE KEY i18n_uniq_idx (code, lang)
);

CREATE TABLE pdt (
    prefix VARCHAR(32) NOT NULL,
    domain VARCHAR(255) NOT NULL,
    UNIQUE KEY pdt_idx (prefix)
);

CREATE TABLE cpl (
    uid VARCHAR(64) NOT NULL,
    cpl_xml BLOB,
    cpl_bin BLOB,
    UNIQUE KEY cpl_key (uid)
);

CREATE TABLE customers (
    cid INT AUTO_INCREMENT NOT NULL,
    name VARCHAR(128) NOT NULL,
    address VARCHAR(255),
    phone VARCHAR(64),
    email VARCHAR(255),
    UNIQUE KEY cu_idx (cid)
);

 