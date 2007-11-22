INSERT INTO version (table_name, table_version) values ('dbaliases','1');
CREATE TABLE dbaliases (
    id SERIAL PRIMARY KEY NOT NULL,
    alias_username VARCHAR(64) NOT NULL DEFAULT '',
    alias_domain VARCHAR(64) NOT NULL DEFAULT '',
    username VARCHAR(64) NOT NULL DEFAULT '',
    domain VARCHAR(64) NOT NULL DEFAULT '',
    CONSTRAINT dbaliases_alias_idx UNIQUE (alias_username, alias_domain)
);

CREATE INDEX dbaliases_target_idx ON dbaliases (username, domain);

