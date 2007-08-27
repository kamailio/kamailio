INSERT INTO version (table_name, table_version) values ('gw','5');
CREATE TABLE gw (
    id INT(10) UNSIGNED AUTO_INCREMENT PRIMARY KEY NOT NULL,
    gw_name VARCHAR(128) NOT NULL,
    grp_id INT UNSIGNED NOT NULL,
    ip_addr VARCHAR(15) NOT NULL,
    port SMALLINT UNSIGNED,
    uri_scheme TINYINT UNSIGNED,
    transport TINYINT UNSIGNED,
    strip TINYINT UNSIGNED,
    prefix VARCHAR(16) DEFAULT NULL,
    dm TINYINT UNSIGNED NOT NULL DEFAULT 1,
    UNIQUE KEY name_gw (gw_name),
    KEY gid_gw (grp_id)
) ENGINE=MyISAM;

INSERT INTO version (table_name, table_version) values ('gw_grp','1');
CREATE TABLE gw_grp (
    grp_id INT UNSIGNED AUTO_INCREMENT PRIMARY KEY NOT NULL,
    grp_name VARCHAR(64) NOT NULL
) ENGINE=MyISAM;

INSERT INTO version (table_name, table_version) values ('lcr','2');
CREATE TABLE lcr (
    id INT(10) UNSIGNED AUTO_INCREMENT PRIMARY KEY NOT NULL,
    prefix VARCHAR(16) NOT NULL,
    from_uri VARCHAR(64) DEFAULT NULL,
    grp_id INT UNSIGNED NOT NULL,
    priority TINYINT UNSIGNED NOT NULL,
    KEY lcr_prefix_idx (prefix),
    KEY lcr_from_uri_idx (from_uri),
    KEY lcr_grp_id_idx (grp_id)
) ENGINE=MyISAM;

