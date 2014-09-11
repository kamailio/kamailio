INSERT INTO version (table_name, table_version) values ('mohqcalls','1');
CREATE TABLE mohqcalls (
    id SERIAL PRIMARY KEY NOT NULL,
    mohq_id INTEGER NOT NULL,
    call_id VARCHAR(100) NOT NULL,
    call_status INTEGER NOT NULL,
    call_from VARCHAR(100) NOT NULL,
    call_contact VARCHAR(100),
    call_time TIMESTAMP WITHOUT TIME ZONE NOT NULL,
    CONSTRAINT mohqcalls_mohqcalls_idx UNIQUE (call_id)
);

INSERT INTO version (table_name, table_version) values ('mohqueues','1');
CREATE TABLE mohqueues (
    id SERIAL PRIMARY KEY NOT NULL,
    name VARCHAR(25) NOT NULL,
    uri VARCHAR(100) NOT NULL,
    mohdir VARCHAR(100),
    mohfile VARCHAR(100) NOT NULL,
    debug INTEGER NOT NULL,
    CONSTRAINT mohqueues_mohqueue_uri_idx UNIQUE (uri),
    CONSTRAINT mohqueues_mohqueue_name_idx UNIQUE (name)
);

