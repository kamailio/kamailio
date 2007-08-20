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

INSERT INTO version (table_name, table_version) values ('active_watchers','6');
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
    version INTEGER NOT NULL DEFAULT 0,
    socket_info VARCHAR(64) NOT NULL,
    local_contact VARCHAR(128) NOT NULL,
    CONSTRAINT tt_watchers UNIQUE (to_tag)
);

CREATE INDEX ue_active_watchers ON active_watchers (pres_uri, event);
CREATE INDEX exp_active_watchers ON active_watchers (expires);

INSERT INTO version (table_name, table_version) values ('watchers','2');
CREATE TABLE watchers (
    id SERIAL PRIMARY KEY NOT NULL,
    p_uri VARCHAR(128) NOT NULL,
    w_user VARCHAR(64) NOT NULL,
    w_domain VARCHAR(64) NOT NULL,
    event VARCHAR(64) NOT NULL DEFAULT 'presence',
    subs_status VARCHAR(64) NOT NULL,
    reason VARCHAR(64),
    inserted_time INTEGER NOT NULL,
    CONSTRAINT udud_watchers UNIQUE (