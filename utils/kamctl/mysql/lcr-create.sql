INSERT INTO version (table_name, table_version) values ('gw','10');
CREATE TABLE gw (
    id INT(10) UNSIGNED AUTO_INCREMENT PRIMARY KEY NOT NULL,
    lcr_id SMALLINT UNSIGNED NOT NULL,
    gw_name VARCHAR(128) NOT NULL,
    grp_id INT UNSIGNED NOT NULL,
    ip_addr VARCHAR(15) NOT NULL,
    hostname VARCHAR(64),
    port SMALLINT UNSIGNED,
    uri_scheme TINYINT UNSIGNED,
    transport TINYINT UNSIGNED,
    strip TINYINT UNSIGNED,
    tag VARCHAR(16) DEFAULT NULL,
    weight INT UNSIGNED,
    flags INT UNSIGNED DEFAULT 0 NOT NULL,
    defunct INT UNSIGNED DEFAULT NULL,
    CONSTRAINT lcr_id_grp_id_gw_name_idx UNIQUE (lcr_id, grp_id, gw_name),
    CONSTRAINT lcr_id_grp_id_ip_addr_idx UNIQUE (lcr_id, grp_id, ip_addr)
) ENGINE=MyISAM;

INSERT INTO version (table_name, table_version) values ('lcr','3');
CREATE TABLE lcr (
    id INT(10) UNSIGNED AUTO_INCREMENT PRIMARY KEY NOT NULL,
    lcr_id SMALLINT UNSIGNED NOT NULL,
    prefix VARCHAR(16) DEFAULT NULL,
    from_uri VARCHAR(64) DEFAULT NULL,
    grp_id INT UNSIGNED NOT NULL,
    priority TINYINT UNSIGNED NOT NULL
) ENGINE=MyISAM;

CREATE INDEX lcr_id_idx ON lcr (lcr_id);

