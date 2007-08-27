INSERT INTO version (table_name, table_version) values ('gw','4');
CREATE TABLE gw (
    id SERIAL PRIMARY KEY NOT NULL,
    gw_name VARCHAR(128) NOT NULL,
    grp_id INTEGER NOT NULL,
    ip_addr VARCHAR(15) NOT NULL,
    port SMALLINT,
    uri_scheme SMALLINT,
    transport SMALLINT,
    strip SMALLINT,
    prefix VARCHAR(16) DEFAULT NULL,
    CONSTRAINT name_gw UNIQUE (gw_name)
);

CREATE INDEX gid_gw ON gw (grp_id);

INSERT INTO version (table_name, table_version) values ('gw_grp','1');
CREATE TABLE gw_grp (
    grp_id SERIAL PRIMARY KEY NOT NULL,
    grp_name VARCHAR(64) NOT NULL
);

INSERT INTO version (table_name, table_version) values ('lcr','2');
CREATE TABLE lcr (
    id SERIAL PRIMARY KEY NOT NULL,
    prefix VARCHAR(16) NOT NULL,
    from_uri VARCHAR(64) DEFAULT NULL,
    grp_id INTEGER NOT NULL,
    priority SMALLINT NOT NULL
);

CREATE INDEX lcr_prefix_idx ON lcr (prefix);
CREATE INDEX lcr_from_uri_idx ON lcr (from_uri);
CREATE INDEX lcr_grp_id_idx ON lcr (grp_id);

