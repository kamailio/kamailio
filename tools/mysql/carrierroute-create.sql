INSERT INTO version (table_name, table_version) values ('carrierroute','1');
CREATE TABLE carrierroute (
    id INT(10) UNSIGNED AUTO_INCREMENT PRIMARY KEY NOT NULL,
    carrier INT(10) UNSIGNED NOT NULL DEFAULT 0,
    scan_prefix VARCHAR(64) NOT NULL DEFAULT '',
    domain VARCHAR(64) NOT NULL DEFAULT '',
    prob FLOAT NOT NULL DEFAULT 0,
    strip INT(11) UNSIGNED NOT NULL DEFAULT 0,
    rewrite_host VARCHAR(128) NOT NULL DEFAULT '',
    rewrite_prefix VARCHAR(64) NOT NULL DEFAULT '',
    rewrite_suffix VARCHAR(64) NOT NULL DEFAULT '',
    comment VARCHAR(255) DEFAULT NULL
) ENGINE=MyISAM;

INSERT INTO version (table_name, table_version) values ('route_tree','1');
CREATE TABLE route_tree (
    id INT(10) UNSIGNED PRIMARY KEY NOT NULL DEFAULT 0,
    carrier VARCHAR(64) DEFAULT NULL
) ENGINE=MyISAM;

