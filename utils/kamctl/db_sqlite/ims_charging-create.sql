CREATE TABLE ro_session (
    id INTEGER PRIMARY KEY NOT NULL,
    hash_entry INTEGER NOT NULL,
    hash_id INTEGER NOT NULL,
    session_id VARCHAR(100) NOT NULL,
    dlg_hash_entry INTEGER NOT NULL,
    dlg_hash_id INTEGER NOT NULL,
    direction INTEGER NOT NULL,
    asserted_identity VARCHAR(100) NOT NULL,
    callee VARCHAR(100) NOT NULL,
    start_time TIMESTAMP WITHOUT TIME ZONE DEFAULT NULL,
    last_event_timestamp TIMESTAMP WITHOUT TIME ZONE DEFAULT NULL,
    reserved_secs INTEGER DEFAULT NULL,
    valid_for INTEGER DEFAULT NULL,
    state INTEGER DEFAULT NULL,
    incoming_trunk_id VARCHAR(20) DEFAULT NULL,
    outgoing_trunk_id VARCHAR(20) DEFAULT NULL,
    rating_group INTEGER DEFAULT NULL,
    service_identifier INTEGER DEFAULT NULL,
    auth_app_id INTEGER NOT NULL,
    auth_session_type INTEGER NOT NULL,
    pani VARCHAR(100) DEFAULT NULL,
    mac VARCHAR(17) DEFAULT NULL,
    app_provided_party VARCHAR(100) DEFAULT NULL,
    is_final_allocation INTEGER NOT NULL,
    origin_host VARCHAR(150) DEFAULT NULL
);

CREATE INDEX ro_session_hash_idx ON ro_session (hash_entry, hash_id);

INSERT INTO version (table_name, table_version) values ('ro_session','3');
