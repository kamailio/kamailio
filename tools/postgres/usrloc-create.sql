INSERT INTO version (table_name, table_version) values ('location','1004');
CREATE TABLE location (
    id SERIAL PRIMARY KEY NOT NULL,
    username VARCHAR(64) NOT NULL DEFAULT '',
    domain VARCHAR(64) DEFAULT NULL,
    contact VARCHAR(255) NOT NULL DEFAULT '',
    received VARCHAR(128) DEFAULT NULL,
    path VARCHAR(128) DEFAULT NULL,
    expires TIMESTAMP WITHOUT TIME ZONE NOT NULL DEFAULT '2020-05-28 21:32:15',
    q REAL NOT NULL DEFAULT 1.0,
    callid VARCHAR(255) NOT NULL DEFAULT 'Default-Call-ID',
    cseq INTEGER NOT NULL DEFAULT 13,
    last_modified TIMESTAMP WITHOUT TIME ZONE NOT NULL DEFAULT '1900-01-01 00:00:01',
    flags INTEGER NOT NULL DEFAULT 0,
    cflags INTEGER NOT NULL DEFAULT 0,
    user_agent VARCHAR(255) NOT NULL DEFAULT '',
    socket VARCHAR(64) DEFAULT NULL,
    methods INTEGER DEFAULT NULL
);

CREATE INDEX location_account_contact_idx ON location (username, domain, contact);

