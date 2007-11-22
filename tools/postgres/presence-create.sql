INSERT INTO version (table_name, table_version) values ('presentity','2');
CREATE TABLE presentity (
    id SERIAL PRIMARY KEY NOT NULL,
    username VARCHAR(64) NOT NULL,
    domain VARCHAR(64) NOT NULL,
    event VARCHAR(64) NOT NULL,
    etag VARCHAR(64) NOT NULL,
    expires INTEGER NOT NULL,
    received_time INTEGER NOT NULL,
    body BYTEA NOT NULL,
    CONSTRAINT presentity_presentity_idx UNIQUE (username, domain, event, etag)
);

INSERT INTO version (table_name, table_version) values ('active_watchers','9');
CREATE TABLE active_watchers (
    id SERIAL PRIMARY KEY NOT NULL,
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
    CONSTRAINT active_watchers_active_watchers_idx UNIQUE (presentity_uri, callid, to_tag, from_tag)
);

INSERT INTO version (table_name, table_version) values ('watchers','3');
CREATE TABLE watchers (
    id SERIAL PRIMARY KEY NOT NULL,
    presentity_uri VARCHAR(128) NOT NULL,
    watcher_username VARCHAR(64) NOT NULL,
    watcher_domain VARCHAR(64) NOT NULL,
    event VARCHAR(64) NOT NULL DEFAULT 'presence',
    status INTEGER NOT NULL,
    reason VARCHAR(64),
    inserted_time INTEGER NOT NULL,
    CONSTRAINT watchers_watcher_idx UNIQUE (presentity_uri, watcher_username, watcher_domain, event)
);

INSERT INTO version (table_name, table_version) values ('xcap','3');
CREATE TABLE xcap (
    id SERIAL PRIMARY KEY NOT NULL,
    username VARCHAR(64) NOT NULL,
    domain VARCHAR(64) NOT NULL,
    doc BYTEA NOT NULL,
    doc_type INTEGER NOT NULL,
    etag VARCHAR(64) NOT NULL,
    source INTEGER NOT NULL,
    doc_uri VARCHAR(128) NOT NULL,
    port INTEGER NOT NULL,
    CONSTRAINT xcap_account_doc_type_idx UNIQUE (username, domain, doc_type, doc_uri)
);

CREATE INDEX xcap_source_idx ON xcap (source);

INSERT INTO version (table_name, table_version) values ('pua','5');
CREATE TABLE pua (
    id SERIAL PRIMARY KEY NOT NULL,
    pres_uri VARCHAR(128) NOT NULL,
    pres_id VARCHAR(64) NOT NULL,
    event INTEGER NOT NULL,
    expires INTEGER NOT NULL,
    desired_expires INTEGER NOT NULL,
    flag INTEGER NOT NULL,
    etag VARCHAR(64) NOT NULL,
    tuple_id VARCHAR(64),
    watcher_uri VARCHAR(128) NOT NULL,
    call_id VARCHAR(64) NOT NULL,
    to_tag VARCHAR(64) NOT NULL,
    from_tag VARCHAR(64) NOT NULL,
    cseq INTEGER NOT NULL,
    record_route TEXT,
    contact VARCHAR(128) NOT NULL,
    version INTEGER NOT NULL,
    extra_headers TEXT NOT NULL
);

