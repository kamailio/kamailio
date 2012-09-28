INSERT INTO version (table_name, table_version) values ('rls_presentity','1');
CREATE TABLE rls_presentity (
    id INTEGER PRIMARY KEY NOT NULL,
    rlsubs_did VARCHAR(255) NOT NULL,
    resource_uri VARCHAR(128) NOT NULL,
    content_type VARCHAR(255) NOT NULL,
    presence_state BLOB NOT NULL,
    expires INTEGER NOT NULL,
    updated INTEGER NOT NULL,
    auth_state INTEGER NOT NULL,
    reason VARCHAR(64) NOT NULL,
    CONSTRAINT rls_presentity_rls_presentity_idx UNIQUE (rlsubs_did, resource_uri)
);

CREATE INDEX rls_presentity_rlsubs_idx ON rls_presentity (rlsubs_did);
CREATE INDEX rls_presentity_updated_idx ON rls_presentity (updated);
CREATE INDEX rls_presentity_expires_idx ON rls_presentity (expires);

INSERT INTO version (table_name, table_version) values ('rls_watchers','3');
CREATE TABLE rls_watchers (
    id INTEGER PRIMARY KEY NOT NULL,
    presentity_uri VARCHAR(128) NOT NULL,
    to_user VARCHAR(64) NOT NULL,
    to_domain VARCHAR(64) NOT NULL,
    watcher_username VARCHAR(64) NOT NULL,
    watcher_domain VARCHAR(64) NOT NULL,
    event VARCHAR(64) DEFAULT 'presence' NOT NULL,
    event_id VARCHAR(64),
    to_tag VARCHAR(64) NOT NULL,
    from_tag VARCHAR(64) NOT NULL,
    callid VARCHAR(255) NOT NULL,
    local_cseq INTEGER NOT NULL,
    remote_cseq INTEGER NOT NULL,
    contact VARCHAR(128) NOT NULL,
    record_route TEXT,
    expires INTEGER NOT NULL,
    status INTEGER DEFAULT 2 NOT NULL,
    reason VARCHAR(64) NOT NULL,
    version INTEGER DEFAULT 0 NOT NULL,
    socket_info VARCHAR(64) NOT NULL,
    local_contact VARCHAR(128) NOT NULL,
    from_user VARCHAR(64) NOT NULL,
    from_domain VARCHAR(64) NOT NULL,
    updated INTEGER NOT NULL,
    CONSTRAINT rls_watchers_rls_watcher_idx UNIQUE (callid, to_tag, from_tag)
);

CREATE INDEX rls_watchers_rls_watchers_update ON rls_watchers (watcher_username, watcher_domain, event);
CREATE INDEX rls_watchers_rls_watchers_expires ON rls_watchers (expires);
CREATE INDEX rls_watchers_updated_idx ON rls_watchers (updated);

