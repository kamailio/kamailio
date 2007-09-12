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
    CONSTRAINT udee_presentity UNIQUE (username, domain, event, etag)
);

INSERT INTO version (table_name, table_version) values ('active_watchers','8');
CREATE TABLE active_watchers (
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
    CONSTRAINT pctt_watchers UNIQUE (pres_uri, callid, to_tag, from_tag)
);

INSERT INTO version (table_name, table_version) values ('watchers','2');
CREATE TABLE watchers (
    id SERIAL PRIMARY KEY NOT NULL,
    p_uri VARCHAR(128) NOT NULL,
    w_user VARCHAR(64) NOT NULL,
    w_domain VARCHAR(64) NOT NULL,
    event VARCHAR(64) NOT NULL DEFAULT 'presence',
    subs_status INTEGER NOT NULL,
    reason VARCHAR(64),
    inserted_time INTEGER NOT NULL,
    CONSTRAINT uude_watchers UNIQUE (p_uri, w_user, w_domain, event)
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
    CONSTRAINT udd_xcap UNIQUE (username, domain, doc_type)
);

CREATE INDEX source_xcap ON xcap (source);

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

