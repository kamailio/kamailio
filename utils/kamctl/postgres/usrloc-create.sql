CREATE TABLE location (
    id SERIAL PRIMARY KEY NOT NULL,
    ruid VARCHAR(64) DEFAULT '' NOT NULL,
    username VARCHAR(64) DEFAULT '' NOT NULL,
    domain VARCHAR(64) DEFAULT NULL,
    contact VARCHAR(512) DEFAULT '' NOT NULL,
    received VARCHAR(128) DEFAULT NULL,
    path VARCHAR(512) DEFAULT NULL,
    expires TIMESTAMP WITHOUT TIME ZONE DEFAULT '2030-05-28 21:32:15' NOT NULL,
    q REAL DEFAULT 1.0 NOT NULL,
    callid VARCHAR(255) DEFAULT 'Default-Call-ID' NOT NULL,
    cseq INTEGER DEFAULT 1 NOT NULL,
    last_modified TIMESTAMP WITHOUT TIME ZONE DEFAULT '2000-01-01 00:00:01' NOT NULL,
    flags INTEGER DEFAULT 0 NOT NULL,
    cflags INTEGER DEFAULT 0 NOT NULL,
    user_agent VARCHAR(255) DEFAULT '' NOT NULL,
    socket VARCHAR(64) DEFAULT NULL,
    methods INTEGER DEFAULT NULL,
    instance VARCHAR(255) DEFAULT NULL,
    reg_id INTEGER DEFAULT 0 NOT NULL,
    server_id INTEGER DEFAULT 0 NOT NULL,
    connection_id INTEGER DEFAULT 0 NOT NULL,
    keepalive INTEGER DEFAULT 0 NOT NULL,
    partition INTEGER DEFAULT 0 NOT NULL,
    CONSTRAINT location_ruid_idx UNIQUE (ruid)
);

CREATE INDEX location_account_contact_idx ON location (username, domain, contact);
CREATE INDEX location_expires_idx ON location (expires);
CREATE INDEX location_connection_idx ON location (server_id, connection_id);

INSERT INTO version (table_name, table_version) values ('location','9');

CREATE TABLE location_attrs (
    id SERIAL PRIMARY KEY NOT NULL,
    ruid VARCHAR(64) DEFAULT '' NOT NULL,
    username VARCHAR(64) DEFAULT '' NOT NULL,
    domain VARCHAR(64) DEFAULT NULL,
    aname VARCHAR(64) DEFAULT '' NOT NULL,
    atype INTEGER DEFAULT 0 NOT NULL,
    avalue VARCHAR(512) DEFAULT '' NOT NULL,
    last_modified TIMESTAMP WITHOUT TIME ZONE DEFAULT '2000-01-01 00:00:01' NOT NULL
);

CREATE INDEX location_attrs_account_record_idx ON location_attrs (username, domain, ruid);
CREATE INDEX location_attrs_last_modified_idx ON location_attrs (last_modified);

INSERT INTO version (table_name, table_version) values ('location_attrs','1');

