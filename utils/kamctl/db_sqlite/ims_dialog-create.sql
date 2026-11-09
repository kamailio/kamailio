CREATE TABLE dialog_in (
    id INTEGER PRIMARY KEY NOT NULL,
    hash_entry INTEGER NOT NULL,
    hash_id INTEGER NOT NULL,
    did VARCHAR(45) NOT NULL,
    callid VARCHAR(255) NOT NULL,
    from_uri VARCHAR(255) NOT NULL,
    from_tag VARCHAR(128) NOT NULL,
    caller_original_cseq VARCHAR(20) NOT NULL,
    req_uri VARCHAR(255) NOT NULL,
    caller_route_set VARCHAR(512),
    caller_contact VARCHAR(255) NOT NULL,
    caller_sock VARCHAR(64) NOT NULL,
    timeout INTEGER DEFAULT 0 NOT NULL,
    state INTEGER NOT NULL,
    start_time INTEGER NOT NULL,
    sflags INTEGER DEFAULT 0 NOT NULL,
    toroute_name VARCHAR(32),
    toroute_index INTEGER NOT NULL
);

CREATE INDEX dialog_in_hash_idx ON dialog_in (hash_entry, hash_id);

INSERT INTO version (table_name, table_version) values ('dialog_in','7');

CREATE TABLE dialog_out (
    id INTEGER PRIMARY KEY NOT NULL,
    hash_entry INTEGER NOT NULL,
    hash_id INTEGER NOT NULL,
    did VARCHAR(45) NOT NULL,
    to_uri VARCHAR(255) NOT NULL,
    to_tag VARCHAR(128) NOT NULL,
    caller_cseq VARCHAR(20) NOT NULL,
    callee_cseq VARCHAR(20) NOT NULL,
    callee_contact VARCHAR(255) NOT NULL,
    callee_route_set VARCHAR(512),
    callee_sock VARCHAR(64) NOT NULL
);

CREATE INDEX dialog_out_hash_idx ON dialog_out (hash_entry, hash_id);

INSERT INTO version (table_name, table_version) values ('dialog_out','7');

CREATE TABLE dialog_vars (
    id INTEGER PRIMARY KEY NOT NULL,
    hash_entry INTEGER NOT NULL,
    hash_id INTEGER NOT NULL,
    dialog_key VARCHAR(128) NOT NULL,
    dialog_value VARCHAR(512) NOT NULL
);

CREATE INDEX dialog_vars_hash_idx ON dialog_vars (hash_entry, hash_id);

INSERT INTO version (table_name, table_version) values ('dialog_vars','1');
