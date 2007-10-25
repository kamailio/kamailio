INSERT INTO version (table_name, table_version) values ('rls_presentity','0');
CREATE TABLE rls_presentity (
    id INT(10) UNSIGNED AUTO_INCREMENT PRIMARY KEY NOT NULL,
    rlsubs_did VARCHAR(512) NOT NULL,
    resource_uri VARCHAR(128) NOT NULL,
    content_type VARCHAR(64) NOT NULL,
    presence_state BLOB NOT NULL,
    expires INT(11) NOT NULL,
    updated INT(11) NOT NULL,
    auth_state INT(11) NOT NULL,
    reason VARCHAR(64) NOT NULL,
    UNIQUE KEY rls_presentity_idx (rlsubs_did, resource_uri),
    UNIQUE KEY updated_idx (updated)
) ENGINE=MyISAM;

INSERT INTO version (table_name, table_version) values ('rls_watchers','1');
CREATE TABLE rls_watchers (
    id INT(10) UNSIGNED AUTO_INCREMENT PRIMARY KEY NOT NULL,
    presentity_uri VARCHAR(128) NOT NULL,
    to_user VARCHAR(64) NOT NULL,
    to_domain VARCHAR(64) NOT NULL,
    watcher_username VARCHAR(64) NOT NULL,
    watcher_domain VARCHAR(64) NOT NULL,
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
    UNIQUE KEY rls_watcher_idx (presentity_uri, callid, to_tag, from_tag)
) ENGINE=MyISAM;

