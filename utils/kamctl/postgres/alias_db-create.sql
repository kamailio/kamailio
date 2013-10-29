INSERT INTO version (table_name, table_version) values ('dbaliases','1');
CREATE TABLE dbaliases (
    id SERIAL PRIMARY KEY NOT NULL,
    alias_username VARCHAR(64) DEFAULT '' NOT NULL,
    alias_domain VARCHAR(64) DEFAULT '' NOT NULL,
    username VARCHAR(64) DEFAULT '' NOT NULL,
    domain VARCHAR(64) DEFAULT '' NOT NULL
);

CREATE INDEX dbaliases_alias_user_idx ON dbaliases (alias_username);
CREATE INDEX dbaliases_alias_idx ON dbaliases (alias_username, alias_domain);
CREATE INDEX dbaliases_target_idx ON dbaliases (username, domain);

