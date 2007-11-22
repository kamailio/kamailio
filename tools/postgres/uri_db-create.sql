INSERT INTO version (table_name, table_version) values ('uri','1');
CREATE TABLE uri (
    id SERIAL PRIMARY KEY NOT NULL,
    username VARCHAR(64) NOT NULL DEFAULT '',
    domain VARCHAR(64) NOT NULL DEFAULT '',
    uri_user VARCHAR(64) NOT NULL DEFAULT '',
    last_modified TIMESTAMP WITHOUT TIME ZONE NOT NULL DEFAULT '1900-01-01 00:00:01',
    CONSTRAINT uri_account_idx UNIQUE (username, domain, uri_user)
);

