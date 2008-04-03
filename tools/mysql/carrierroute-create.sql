INSERT INTO version (table_name, table_version) values ('carrierroute','2');
CREATE TABLE carrierroute (
    id INT(10) UNSIGNED AUTO_INCREMENT PRIMARY KEY NOT NULL,
    carrier INT(10) UNSIGNED NOT NULL DEFAULT 0,
    domain VARCHAR(64) NOT NULL DEFAULT '',
    scan_prefix VARCHAR(64) NOT NULL DEFAULT '',
    flags INT(11) UNSIGNED NOT NULL DEFAULT 0,
    mask INT(11) UNSIGNED NOT NULL DEFAULT 0,
    prob FLOAT NOT NULL DEFAULT 0,
    strip INT(11) UNSIGNED NOT NULL DEFAULT 0,
    rewrite_host VARCHAR(128) NOT NULL DEFAULT '',
    rewrite_prefix VARCHAR(64) NOT NULL DEFAULT '',
    rewrite_suffix VARCHAR(64) NOT NULL DEFAULT '',
    description VARCHAR(255) DEFAULT NULL
) ENGINE=MyISAM;

INSERT INTO version (table_name, table_version) values ('carrierfailureroute','1');
CREATE TABLE carrierfailureroute (
    id INT(10) UNSIGNED AUTO_INCREMENT PRIMARY KEY NOT NULL,
    carrier INT(10) UNSIGNED NOT NULL DEFAULT 0,
    domain VARCHAR(64) NOT NULL DEFAULT '',
    scan_prefix VARCHAR(64) NOT NULL DEFAULT '',
    host_name VARCHAR(128) NOT NULL DEFAULT '',
    reply_code VARCHAR(3) NOT NULL DEFAULT '',
    flags INT(11) UNSIGNED NOT NULL DEFAULT 0,
    mask INT(11) UNSIGNED NOT NULL DEFAULT 0,
    next_domain VARCHAR(64) NOT NULL DEFAULT '',
    description VARCHAR(255) DEFAULT NULL
) ENGINE=MyISAM;

INSERT INTO version (table_name, table_version) values ('route_tree','1');
CREATE TABLE route_tree (
    id INT(10) UNSIGNED AUTO_INCREMENT PRIMARY KEY NOT NULL,
    carrier VARCHAR(64) DEFAULT NULL
) ENGINE=MyISAM;

