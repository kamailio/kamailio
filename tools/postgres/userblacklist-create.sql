INSERT INTO version (table_name, table_version) values ('userblacklist','1');
CREATE TABLE userblacklist (
    id SERIAL PRIMARY KEY NOT NULL,
    username VARCHAR(64) NOT NULL DEFAULT '',
    domain VARCHAR(64) NOT NULL DEFAULT '',
    prefix VARCHAR(64) NOT NULL DEFAULT '',
    whitelist INTEGER NOT NULL DEFAULT 0,
    description VARCHAR(64) NOT NULL DEFAULT ''
);

CREATE INDEX userblacklist_userblacklist_idx ON userblacklist (username, domain, prefix);

INSERT INTO version (table_name, table_version) values ('globalblacklist','1');
CREATE TABLE globalblacklist (
    id SERIAL PRIMARY KEY NOT NULL,
    prefix VARCHAR(64) NOT NULL DEFAULT '',
    whitelist INTEGER NOT NULL DEFAULT 0,
    description VARCHAR(64) NOT NULL DEFAULT ''
);

CREATE INDEX globalblacklist_userblacklist_idx ON globalblacklist (prefix);

