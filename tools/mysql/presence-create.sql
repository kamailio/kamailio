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
    UNIQUE KEY presentity_idx (username, domain, event, etag)
) ENGINE=MyISAM;

INSERT INTO version (table_name, table_version) values ('active_watchers','9');
CREATE TABLE active_watchers (
    id INT(10) UNSIGNED AUTO_INCREMENT PRIMARY KEY NOT NULL,
    presentity_uri VARCHAR(128) NOT NULL,
    watcher_username VARCHAR(64) NOT NULL,
    watcher_domain VARCHAR(64) NOT NULL,
    to_user VARCHAR(64) NOT NULL,
    to_domain VARCHAR(64) NOT NULL,
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
    UNIQUE KEY active_watchers_idx (presentity_uri, callid, to_tag, from_tag)
) ENGINE=MyISAM;

INSERT INTO version (table_name, table_version) values ('watchers','3');
CREATE TABLE watchers (
    id INT(10) UNSIGNED AUTO_INCREMENT PRIMARY KEY NOT NULL,
    presentity_uri VARCHAR(128) NOT NULL,
    watcher_username VARCHAR(64) NOT NULL,
    watcher_domain VARCHAR(64) NOT NULL,
    event VARCHAR(64) NOT NULL DEFAULT 'presence',
    status INT(11) NOT NULL,
    reason VARCHAR(64),
    inserted_time INT(11) NOT NULL,
    UNIQUE KEY watcher_idx (presentity_uri, watcher_username, watcher_domain, event)
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
    UNIQUE KEY account_doc_type_idx (username, domain, doc_type, doc_uri),
    KEY source_idx (source)
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

