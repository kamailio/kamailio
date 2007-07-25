INSERT INTO version (table_name, table_version) values ('dbaliases','1');
CREATE TABLE dbaliases (
    id SERIAL PRIMARY KEY NOT NULL,
    alias_username VARCHAR(64) NOT NULL DEFAULT '',
    alias_domain VARCHAR(64) NOT NULL DEFAULT '',
    username VARCHAR(64) NOT NULL DEFAULT '',
    domain VARCHAR(64) NOT NULL DEFAULT '',
    CONSTRAINT alias_key UNIQUE (alias_username, alias_domain)
);

CREATE INDEX alias_user ON dbaliases (username, domain);

 