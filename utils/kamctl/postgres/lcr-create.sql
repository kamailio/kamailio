INSERT INTO version (table_name, table_version) values ('lcr_gw','3');
CREATE TABLE lcr_gw (
    id SERIAL PRIMARY KEY NOT NULL,
    lcr_id SMALLINT NOT NULL,
    gw_name VARCHAR(128),
    ip_addr VARCHAR(50),
    hostname VARCHAR(64),
    port SMALLINT,
    params VARCHAR(64),
    uri_scheme SMALLINT,
    transport SMALLINT,
    strip SMALLINT,
    prefix VARCHAR(16) DEFAULT NULL,
    tag VARCHAR(64) DEFAULT NULL,
    flags INTEGER DEFAULT 0 NOT NULL,
    defunct INTEGER DEFAULT NULL
);

CREATE INDEX lcr_gw_lcr_id_idx ON lcr_gw (lcr_id);

INSERT INTO version (table_name, table_version) values ('lcr_rule_target','1');
CREATE TABLE lcr_rule_target (
    id SERIAL PRIMARY KEY NOT NULL,
    lcr_id SMALLINT NOT NULL,
    rule_id INTEGER NOT NULL,
    gw_id INTEGER NOT NULL,
    priority SMALLINT NOT NULL,
    weight INTEGER DEFAULT 1 NOT NULL,
    CONSTRAINT lcr_rule_target_rule_id_gw_id_idx UNIQUE (rule_id, gw_id)
);

CREATE INDEX lcr_rule_target_lcr_id_idx ON lcr_rule_target (lcr_id);

INSERT INTO version (table_name, table_version) values ('lcr_rule','2');
CREATE TABLE lcr_rule (
    id SERIAL PRIMARY KEY NOT NULL,
    lcr_id SMALLINT NOT NULL,
    prefix VARCHAR(16) DEFAULT NULL,
    from_uri VARCHAR(64) DEFAULT NULL,
    request_uri VARCHAR(64) DEFAULT NULL,
    stopper INTEGER DEFAULT 0 NOT NULL,
    enabled INTEGER DEFAULT 1 NOT NULL,
    CONSTRAINT lcr_rule_lcr_id_prefix_from_uri_idx UNIQUE (lcr_id, prefix, from_uri)
);

