INSERT INTO version (table_name, table_version) values ('domain','1');
CREATE TABLE domain (
    id SERIAL PRIMARY KEY NOT NULL,
    domain VARCHAR(64) NOT NULL DEFAULT '',
    last_modified TIMESTAMP WITHOUT TIME ZONE NOT NULL DEFAULT '1900-01-01 00:00:01',
    CONSTRAINT domain_domain_idx UNIQUE (domain)
);

