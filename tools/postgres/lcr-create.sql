INSERT INTO version (table_name, table_version) values ('gw','9');
CREATE TABLE gw (
    id SERIAL PRIMARY KEY NOT NULL,
    gw_name VARCHAR(128) NOT NULL,
    grp_id INTEGER NOT NULL,
    ip_addr VARCHAR(15) NOT NULL,
    hostname VARCHAR(64),
    port SMALLINT,
    uri_scheme SMALLINT,
    transport SMALLINT,
    strip SMALLINT,
    tag VARCHAR(16) DEFAULT NULL,
    weight INTEGER,
    ping SMALLINT DEFAULT 0 NOT NULL,
    flags INTEGER DEFAULT 0 NOT NULL,
    CONSTRAINT gw_gw_name_idx UNIQUE (gw_name)
);

CREATE INDEX gw_grp_id_idx ON gw (grp_id);

INSERT INTO version (table_name, table_version) values ('lcr','2');
CREATE TABLE lcr (
    id SERIAL PRIMARY KEY NOT NULL,
    prefix VARCHAR(16) DEFAULT NULL,
    from_uri VARCHAR(64) DEFAULT NULL,
    grp_id INTEGER NOT NULL,
    priority SMALLINT NOT NULL
);

CREATE INDEX lcr_prefix_idx ON lcr (prefix);
CREATE INDEX lcr_from_uri_idx ON lcr (from_uri);
CREATE INDEX lcr_grp_id_idx ON lcr (grp_id);

