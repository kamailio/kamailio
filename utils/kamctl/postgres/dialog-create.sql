INSERT INTO version (table_name, table_version) values ('dialog','7');
CREATE TABLE dialog (
    id SERIAL PRIMARY KEY NOT NULL,
    hash_entry INTEGER NOT NULL,
    hash_id INTEGER NOT NULL,
    callid VARCHAR(255) NOT NULL,
    from_uri VARCHAR(128) NOT NULL,
    from_tag VARCHAR(64) NOT NULL,
    to_uri VARCHAR(128) NOT NULL,
    to_tag VARCHAR(64) NOT NULL,
    caller_cseq VARCHAR(20) NOT NULL,
    callee_cseq VARCHAR(20) NOT NULL,
    caller_route_set VARCHAR(512),
    callee_route_set VARCHAR(512),
    caller_contact VARCHAR(128) NOT NULL,
    callee_contact VARCHAR(128) NOT NULL,
    caller_sock VARCHAR(64) NOT NULL,
    callee_sock VARCHAR(64) NOT NULL,
    state INTEGER NOT NULL,
    start_time INTEGER NOT NULL,
    timeout INTEGER DEFAULT 0 NOT NULL,
    sflags INTEGER DEFAULT 0 NOT NULL,
    iflags INTEGER DEFAULT 0 NOT NULL,
    toroute_name VARCHAR(32),
    req_uri VARCHAR(128) NOT NULL,
    xdata VARCHAR(512)
);

CREATE INDEX dialog_hash_idx ON dialog (hash_entry, hash_id);

INSERT INTO version (table_name, table_version) values ('dialog_vars','1');
CREATE TABLE dialog_vars (
    id SERIAL PRIMARY KEY NOT NULL,
    hash_entry INTEGER NOT NULL,
    hash_id INTEGER NOT NULL,
    dialog_key VARCHAR(128) NOT NULL,
    dialog_value VARCHAR(512) NOT NULL
);

CREATE INDEX dialog_vars_hash_idx ON dialog_vars (hash_entry, hash_id);

