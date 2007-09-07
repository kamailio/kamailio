INSERT INTO version (table_name, table_version) values ('presentity','2');
CREATE TABLE presentity (
    id INT(10) UNSIGNED AUTO_INCREMENT PRIMARY KEY NOT NULL,
    username VARCHAR(64) NOT NULL,
    domain VARCHAR(64) NOT NULL,
    event VARCHAR(64) NOT NULL,
    etag VARCHAR(64) NOT NULL,
    expires INT(11) NOT NULL,
    received_time INT(11) NOT NULL,
    body BLOB NOT NULL,
    UNIQUE KEY udee_presentity (username, domain, event, etag)
) ENGINE=MyISAM;

INSERT INTO version (table_name, table_version) values ('active_watchers','7');
CREATE TABLE active_watchers (
    id INT(10) UNSIGNED AUTO_INCREMENT PRIMARY KEY NOT NULL,
    pres_uri VARCHAR(128) NOT NULL,
    to_user VARCHAR(64) NOT NULL,
    to_domain VARCHAR(64) NOT NULL,
    from_user VARCHAR(64) NOT NULL,
    from_domain VARCHAR(64) NOT NULL,
    event VARCHAR(64) NOT NULL DEFAULT 'presence',
    event_id VARCHAR(64),
    to_tag VARCHAR(64) NOT NULL,
    from_tag VARCHAR(64) NOT NULL,
    callid VARCHAR(64) NOT NULL,
    local_cseq INT(11) NOT NULL,
    remote_cseq INT(11) NOT NULL,
    contact VARCHAR(64) NOT NULL,
    record_route TEXT,
    expires INT(11) NOT NULL,
    status INT(11) NOT NULL DEFAULT 2,
    reason VARCHAR(64) NOT NULL,
    version INT(11) NOT NULL DEFAULT 0,
    socket_info VARCHAR(64) NOT NULL,
    local_contact VARCHAR(128) NOT NULL,
    UNIQUE KEY tt_watchers (to_tag),
    KEY ue_active_watchers (pres_uri, event),
    KEY exp_active_watchers (expires)
) ENGINE=MyISAM;

INSERT INTO version (table_name, table_version) values ('watchers','2');
CREATE TABLE watchers (
    id INT(10) UNSIGNED AUTO_INCREMENT PRIMARY KEY NOT NULL,
    p_uri VARCHAR(128) NOT NULL,
    w_user VARCHAR(64) NOT NULL,
    w_domain VARCHAR(64) NOT NULL,
    event VARCHAR(64) NOT NULL DEFAULT 'presence',
    subs_status INT(11) NOT NULL,
    reason VARCHAR(64),
    inserted_time INT(11) NOT NULL,
    UNIQUE KEY uude_watchers (p_uri, w_user, w_domain, event)
) ENGINE=MyISAM;

INSERT INTO version (table_name, table_version) values ('xcap','3');
CREATE TABLE xcap (
    id INT(10) UNSIGNED AUTO_INCREMENT PRIMARY KEY NOT NULL,
    username VARCHAR(64) NOT NULL,
    domain VARCHAR(64) NOT NULL,
    doc BLOB NOT NULL,
    doc_type INT(11) NOT NULL,
    etag VARCHAR(64) NOT NULL,
    source INT(11) NOT NULL,
    doc_uri VARCHAR(128) NOT NULL,
    port INT(11) NOT NULL,
    UNIQUE KEY udd_xcap (username, domain, doc_type),
    KEY source_xcap (source)
) ENGINE=MyISAM;

INSERT INTO version (table_name, table_version) values ('pua','5');
CREATE TABLE pua (
    id INT(10) UNSIGNED AUTO_INCREMENT PRIMARY KEY NOT NULL,
    pres_uri VARCHAR(128) NOT NULL,
    pres_id VARCHAR(64) NOT NULL,
    event INT(11) NOT NULL,
    expires INT(11) NOT NULL,
    desired_expires INT(11) NOT NULL,
    flag INT(11) NOT NULL,
    etag VARCHAR(64) NOT NULL,
    tuple_id VARCHAR(64),
    watcher_uri VARCHAR(128) NOT NULL,
    call_id VARCHAR(64) NOT NULL,
    to_tag VARCHAR(64) NOT NULL,
    from_tag VARCHAR(64) NOT NULL,
    cseq INT(11) NOT NULL,
    record_route TEXT,
    contact VARCHAR(128) NOT NULL,
    version INT(11) NOT NULL,
    extra_headers TEXT NOT NULL
) ENGINE=MyISAM;

