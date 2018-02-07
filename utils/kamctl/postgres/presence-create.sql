CREATE TABLE presentity (
    id SERIAL PRIMARY KEY NOT NULL,
    username VARCHAR(64) NOT NULL,
    domain VARCHAR(64) NOT NULL,
    event VARCHAR(64) NOT NULL,
    etag VARCHAR(64) NOT NULL,
    expires INTEGER NOT NULL,
    received_time INTEGER NOT NULL,
    body BYTEA NOT NULL,
    sender VARCHAR(128) NOT NULL,
    priority INTEGER DEFAULT 0 NOT NULL,
    CONSTRAINT presentity_presentity_idx UNIQUE (username, domain, event, etag)
);

CREATE INDEX presentity_presentity_expires ON presentity (expires);
CREATE INDEX presentity_account_idx ON presentity (username, domain, event);

INSERT INTO version (table_name, table_version) values ('presentity','4');

CREATE TABLE active_watchers (
    id SERIAL PRIMARY KEY NOT NULL,
    presentity_uri VARCHAR(128) NOT NULL,
    watcher_username VARCHAR(64) NOT NULL,
    watcher_domain VARCHAR(64) NOT NULL,
    to_user VARCHAR(64) NOT NULL,
    to_domain VARCHAR(64) NOT NULL,
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
    reason VARCHAR(64),
    version INTEGER DEFAULT 0 NOT NULL,
    socket_info VARCHAR(64) NOT NULL,
    local_contact VARCHAR(128) NOT NULL,
    from_user VARCHAR(64) NOT NULL,
    from_domain VARCHAR(64) NOT NULL,
    updated INTEGER NOT NULL,
    updated_winfo INTEGER NOT NULL,
    flags INTEGER DEFAULT 0 NOT NULL,
    user_agent VARCHAR(255) DEFAULT '',
    CONSTRAINT active_watchers_active_watchers_idx UNIQUE (callid, to_tag, from_tag)
);

CREATE INDEX active_watchers_active_watchers_expires ON active_watchers (expires);
CREATE INDEX active_watchers_active_watchers_pres ON active_watchers (presentity_uri, event);
CREATE INDEX active_watchers_updated_idx ON active_watchers (updated);
CREATE INDEX active_watchers_updated_winfo_idx ON active_watchers (updated_winfo, presentity_uri);

INSERT INTO version (table_name, table_version) values ('active_watchers','12');

CREATE TABLE watchers (
    id SERIAL PRIMARY KEY NOT NULL,
    presentity_uri VARCHAR(128) NOT NULL,
    watcher_username VARCHAR(64) NOT NULL,
    watcher_domain VARCHAR(64) NOT NULL,
    event VARCHAR(64) DEFAULT 'presence' NOT NULL,
    status INTEGER NOT NULL,
    reason VARCHAR(64),
    inserted_time INTEGER NOT NULL,
    CONSTRAINT watchers_watcher_idx UNIQUE (presentity_uri, watcher_username, watcher_domain, event)
);

INSERT INTO version (table_name, table_version) values ('watchers','3');

CREATE TABLE xcap (
    id SERIAL PRIMARY KEY NOT NULL,
    username VARCHAR(64) NOT NULL,
    domain VARCHAR(64) NOT NULL,
    doc BYTEA NOT NULL,
    doc_type INTEGER NOT NULL,
    etag VARCHAR(64) NOT NULL,
    source INTEGER NOT NULL,
    doc_uri VARCHAR(255) NOT NULL,
    port INTEGER NOT NULL,
    CONSTRAINT xcap_doc_uri_idx UNIQUE (doc_uri)
);

CREATE INDEX xcap_account_doc_type_idx ON xcap (username, domain, doc_type);
CREATE INDEX xcap_account_doc_type_uri_idx ON xcap (username, domain, doc_type, doc_uri);
CREATE INDEX xcap_account_doc_uri_idx ON xcap (username, domain, doc_uri);

INSERT INTO version (table_name, table_version) values ('xcap','4');

CREATE TABLE pua (
    id SERIAL PRIMARY KEY NOT NULL,
    pres_uri VARCHAR(128) NOT NULL,
    pres_id VARCHAR(255) NOT NULL,
    event INTEGER NOT NULL,
    expires INTEGER NOT NULL,
    desired_expires INTEGER NOT NULL,
    flag INTEGER NOT NULL,
    etag VARCHAR(64) NOT NULL,
    tuple_id VARCHAR(64),
    watcher_uri VARCHAR(128) NOT NULL,
    call_id VARCHAR(255) NOT NULL,
    to_tag VARCHAR(64) NOT NULL,
    from_tag VARCHAR(64) NOT NULL,
    cseq INTEGER NOT NULL,
    record_route TEXT,
    contact VARCHAR(128) NOT NULL,
    remote_contact VARCHAR(128) NOT NULL,
    version INTEGER NOT NULL,
    extra_headers TEXT NOT NULL,
    CONSTRAINT pua_pua_idx UNIQUE (etag, tuple_id, call_id, from_tag)
);

CREATE INDEX pua_expires_idx ON pua (expires);
CREATE INDEX pua_dialog1_idx ON pua (pres_id, pres_uri);
CREATE INDEX pua_dialog2_idx ON pua (call_id, from_tag);
CREATE INDEX pua_record_idx ON pua (pres_id);

INSERT INTO version (table_name, table_version) values ('pua','7');

