CREATE TABLE userblocklist (
    id INTEGER PRIMARY KEY NOT NULL,
    username VARCHAR(64) DEFAULT '' NOT NULL,
    domain VARCHAR(64) DEFAULT '' NOT NULL,
    prefix VARCHAR(64) DEFAULT '' NOT NULL,
    allowlist SMALLINT DEFAULT 0 NOT NULL
);

CREATE INDEX userblocklist_userblocklist_idx ON userblocklist (username, domain, prefix);

INSERT INTO version (table_name, table_version) values ('userblocklist','1');

CREATE TABLE globalblocklist (
    id INTEGER PRIMARY KEY NOT NULL,
    prefix VARCHAR(64) DEFAULT '' NOT NULL,
    allowlist SMALLINT DEFAULT 0 NOT NULL,
    description VARCHAR(255) DEFAULT NULL
);

CREATE INDEX globalblocklist_globalblocklist_idx ON globalblocklist (prefix);

INSERT INTO version (table_name, table_version) values ('globalblocklist','1');

