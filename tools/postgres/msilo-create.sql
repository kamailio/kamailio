INSERT INTO version (table_name, table_version) values ('silo','5');
CREATE TABLE silo (
    id SERIAL PRIMARY KEY NOT NULL,
    src_addr VARCHAR(128) NOT NULL DEFAULT '',
    dst_addr VARCHAR(128) NOT NULL DEFAULT '',
    username VARCHAR(64) NOT NULL DEFAULT '',
    domain VARCHAR(64) NOT NULL DEFAULT '',
    inc_time INTEGER NOT NULL DEFAULT 0,
    exp_time INTEGER NOT NULL DEFAULT 0,
    snd_time INTEGER NOT NULL DEFAULT 0,
    ctype VARCHAR(32) NOT NULL DEFAULT 'text/plain',
    body BYTEA NOT NULL DEFAULT ''
);

CREATE INDEX silo_account_idx ON silo (username, domain);

