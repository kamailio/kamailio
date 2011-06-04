INSERT INTO version (table_name, table_version) values ('trusted','5');
CREATE TABLE trusted (
    id INTEGER PRIMARY KEY NOT NULL,
    src_ip VARCHAR(50) NOT NULL,
    proto VARCHAR(4) NOT NULL,
    from_pattern VARCHAR(64) DEFAULT NULL,
    tag VARCHAR(64)
);

CREATE INDEX trusted_peer_idx ON trusted (src_ip);

INSERT INTO version (table_name, table_version) values ('address','4');
CREATE TABLE address (
    id INTEGER PRIMARY KEY NOT NULL,
    grp SMALLINT DEFAULT 1 NOT NULL,
    ip_addr VARCHAR(15) NOT NULL,
    mask SMALLINT DEFAULT 32 NOT NULL,
    port SMALLINT DEFAULT 0 NOT NULL,
    tag VARCHAR(64)
);

