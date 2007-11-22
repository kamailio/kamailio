INSERT INTO version (table_name, table_version) values ('trusted','4');
CREATE TABLE trusted (
    id SERIAL PRIMARY KEY NOT NULL,
    src_ip VARCHAR(50) NOT NULL,
    proto VARCHAR(4) NOT NULL,
    from_pattern VARCHAR(64) DEFAULT NULL,
    tag VARCHAR(32)
);

CREATE INDEX trusted_peer_idx ON trusted (src_ip);

INSERT INTO version (table_name, table_version) values ('address','3');
CREATE TABLE address (
    id SERIAL PRIMARY KEY NOT NULL,
    grp SMALLINT NOT NULL DEFAULT 0,
    ip_addr VARCHAR(15) NOT NULL,
    mask SMALLINT NOT NULL DEFAULT 32,
    port SMALLINT NOT NULL DEFAULT 0
);

