INSERT INTO version (table_name, table_version) values ('uri','1');
CREATE TABLE uri (
    id INTEGER PRIMARY KEY NOT NULL,
    username VARCHAR(64) DEFAULT '' NOT NULL,
    domain VARCHAR(64) DEFAULT '' NOT NULL,
    uri_user VARCHAR(64) DEFAULT '' NOT NULL,
    last_modified TIMESTAMP WITHOUT TIME ZONE DEFAULT '1900-01-01 00:00:01' NOT NULL,
    CONSTRAINT uri_account_idx UNIQUE (username, domain, uri_user)
);

