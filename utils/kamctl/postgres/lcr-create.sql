INSERT INTO version (table_name, table_version) values ('gw','10');
CREATE TABLE gw (
    id SERIAL PRIMARY KEY NOT NULL,
    lcr_id SMALLINT NOT NULL,
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
    flags INTEGER DEFAULT 0 NOT NULL,
    defunct INTEGER DEFAULT NULL,
    CONSTRAINT gw_lcr_id_grp_id_gw_name_idx UNIQUE (lcr_id, grp_id, gw_name),
    CONSTRAINT gw_lcr_id_grp_id_ip_addr_idx UNIQUE (lcr_id, grp_id, ip_addr)
);

INSERT INTO version (table_name, table_version) values ('lcr','3');
CREATE TABLE lcr (
    id SERIAL PRIMARY KEY NOT NULL,
    lcr_id SMALLINT NOT NULL,
    prefix VARCHAR(16) DEFAULT NULL,
    from_uri VARCHAR(64) DEFAULT NULL,
    grp_id INTEGER NOT NULL,
    priority SMALLINT NOT NULL
);

CREATE INDEX lcr_lcr_id_idx ON lcr (lcr_id);

