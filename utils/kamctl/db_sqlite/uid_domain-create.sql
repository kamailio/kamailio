INSERT INTO version (table_name, table_version) values ('uid_domain','2');
CREATE TABLE uid_domain (
    id INTEGER PRIMARY KEY NOT NULL,
    did VARCHAR(64) NOT NULL,
    domain VARCHAR(64) NOT NULL,
    flags INTEGER DEFAULT 0 NOT NULL,
    CONSTRAINT uid_domain_domain_idx UNIQUE (domain)
);

CREATE INDEX uid_domain_did_idx ON uid_domain (did);

INSERT INTO version (table_name, table_version) values ('uid_domain_attrs','1');
CREATE TABLE uid_domain_attrs (
    id INTEGER PRIMARY KEY NOT NULL,
    did VARCHAR(64),
    name VARCHAR(32) NOT NULL,
    type INTEGER DEFAULT 0 NOT NULL,
    value VARCHAR(128),
    flags INTEGER DEFAULT 0 NOT NULL,
    CONSTRAINT uid_domain_attrs_domain_attr_idx UNIQUE (did, name, value)
);

CREATE INDEX uid_domain_attrs_domain_did ON uid_domain_attrs (did, flags);

