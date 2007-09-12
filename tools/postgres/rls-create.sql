INSERT INTO version (table_name, table_version) values ('rls_presentity','0');
CREATE TABLE rls_presentity (
    id SERIAL PRIMARY KEY NOT NULL,
    rlsubs_did VARCHAR(512) NOT NULL,
    resource_uri VARCHAR(128) NOT NULL,
    content_type VARCHAR(64) NOT NULL,
    presence_state BYTEA NOT NULL,
    expires INTEGER NOT NULL,
    updated INTEGER NOT NULL,
    auth_state INTEGER NOT NULL,
    reason VARCHAR(64) NOT NULL,
    CONSTRAINT udid_rlspres UNIQUE (rlsubs_did, resource_uri),
    CONSTRAINT u_rlspres UNIQUE (updated)
);

INSERT INTO version (table_name, table_version) values ('rls_watchers','0');
CREATE TABLE rls_watchers (
    id SERIAL PRIMARY KEY NOT NULL,
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
    local_cseq INTEGER NOT NULL,
    remote_cseq INTEGER NOT NULL,
    contact VARCHAR(64) NOT NULL,
    record_route TEXT,
    expires INTEGER NOT NULL,
    status INTEGER NOT NULL DEFAULT 2,
    reason VARCHAR(64) NOT NULL,
    version INTEGER NOT NULL DEFAULT 0,
    socket_info VARCHAR(64) NOT NULL,
    local_contact VARCHAR(128) NOT NULL,
    CONSTRAINT pctt_rlwatchers UNIQUE (pres_uri, callid, to_tag, from_tag)
);

