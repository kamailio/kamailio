INSERT INTO version (table_name, table_version) values ('domain','2');
CREATE TABLE domain (
    id INTEGER PRIMARY KEY NOT NULL,
    domain VARCHAR(64) NOT NULL,
    did VARCHAR(64) DEFAULT NULL,
    last_modified TIMESTAMP WITHOUT TIME ZONE DEFAULT '1900-01-01 00:00:01' NOT NULL,
    CONSTRAINT domain_domain_idx UNIQUE (domain)
);

INSERT INTO version (table_name, table_version) values ('domain_attrs','1');
CREATE TABLE domain_attrs (
    id INTEGER PRIMARY KEY NOT NULL,
    did VARCHAR(64) NOT NULL,
    name VARCHAR(32) NOT NULL,
    type INTEGER NOT NULL,
    value VARCHAR(255) NOT NULL,
    last_modified TIMESTAMP WITHOUT TIME ZONE DEFAULT '1900-01-01 00:00:01' NOT NULL,
    CONSTRAINT domain_attrs_domain_attrs_idx UNIQUE (did, name, value)
);

