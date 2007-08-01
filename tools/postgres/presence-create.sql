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

INSERT INTO version (table_name, table_version) values ('active_watchers','5');
CREATE TABLE active_watchers (
    id SERIAL PRIMARY KEY NOT NULL,
    pres_user VARCHAR(64) NOT NULL,
    pres_domain VARCHAR(64) NOT NULL,
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
    status VARCHAR(32) NOT NULL DEFAULT 'pending',
    version INTEGER NOT NULL DEFAULT 0,
    socket_info VARCHAR(64) NOT NULL,
    local_contact VARCHAR(128) NOT NULL,
    CONSTRAINT tt_watchers UNIQUE (to_tag)
);

CREATE INDEX ude_active_watchers ON active_watchers (pres_domain, pres_user, event);
CREATE INDEX exp_active_watchers ON active_watchers (expires);

INSERT INTO version (table_name, table_version) values ('watchers','1');
CREATE TABLE watchers (
    id SERIAL PRIMARY KEY NOT NULL,
    p_user VARCHAR(64) NOT NULL,
    p_domain VARCHAR(64) NOT NULL,
    w_user VARCHAR(64) NOT NULL,
    w_domain VARCHAR(64) NOT NULL,
    subs_status VARCHAR(64) NOT NULL,
    reason VARCHAR(64),
    inserted_time INTEGER NOT NULL,
    CONSTRAINT udud_watchers UNIQUE (p_user, p_domain, w_user, w_domain)
);

INSERT INTO version (table_name, table_version) values ('xcap_xml','1');
CREATE TABLE xcap_xml (
    id SERIAL PRIMARY KEY NOT NULL,
    username VARCHAR(64) NOT NULL,
    domain VARCHAR(64) NOT NULL,
    xcap BYTEA NOT NULL,
    doc_type INTEGER NOT NULL,
    CONSTRAINT udd_xcap UNIQUE (username, domain, doc_type)
);

INSERT INTO version (table_name, table_version) values ('pua','4');
CREATE TABLE pua (
    id SERIAL PRIMARY KEY NOT NULL,
    pres_uri VARCHAR(128) NOT NULL,
    pres_id VARCHAR(64) NOT NULL,
    event INTEGER NOT NULL,
    expires INTEGER NOT NULL,
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
    version INTEGER NOT NULL
);

 