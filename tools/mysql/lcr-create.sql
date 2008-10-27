INSERT INTO version (table_name, table_version) values ('gw','8');
CREATE TABLE gw (
    id INT(10) UNSIGNED AUTO_INCREMENT PRIMARY KEY NOT NULL,
    gw_name VARCHAR(128) NOT NULL,
    grp_id INT UNSIGNED NOT NULL,
    ip_addr VARCHAR(15) NOT NULL,
    hostname VARCHAR(64) NOT NULL,
    port SMALLINT UNSIGNED,
    uri_scheme TINYINT UNSIGNED,
    transport TINYINT UNSIGNED,
    strip TINYINT UNSIGNED,
    tag VARCHAR(16) DEFAULT NULL,
    weight INT UNSIGNED,
    flags INT UNSIGNED DEFAULT 0 NOT NULL,
    CONSTRAINT gw_name_idx UNIQUE (gw_name)
) ENGINE=MyISAM;

CREATE INDEX grp_id_idx ON gw (grp_id);

INSERT INTO version (table_name, table_version) values ('lcr','2');
CREATE TABLE lcr (
    id INT(10) UNSIGNED AUTO_INCREMENT PRIMARY KEY NOT NULL,
    prefix VARCHAR(16) DEFAULT NULL,
    from_uri VARCHAR(64) DEFAULT NULL,
    grp_id INT UNSIGNED NOT NULL,
    priority TINYINT UNSIGNED NOT NULL
) ENGINE=MyISAM;

CREATE INDEX prefix_idx ON lcr (prefix);
CREATE INDEX from_uri_idx ON lcr (from_uri);
CREATE INDEX grp_id_idx ON lcr (grp_id);

