INSERT INTO version (table_name, table_version) values ('grp','2');
CREATE TABLE grp (
    id INT(10) UNSIGNED AUTO_INCREMENT PRIMARY KEY NOT NULL,
    username VARCHAR(64) NOT NULL DEFAULT '',
    domain VARCHAR(64) NOT NULL DEFAULT '',
    grp VARCHAR(64) NOT NULL DEFAULT '',
    last_modified DATETIME NOT NULL DEFAULT '1900-01-01 00:00:01',
    UNIQUE KEY account_group_idx (username, domain, grp)
) ENGINE=MyISAM;

INSERT INTO version (table_name, table_version) values ('re_grp','1');
CREATE TABLE re_grp (
    id INT(10) UNSIGNED AUTO_INCREMENT PRIMARY KEY NOT NULL,
    reg_exp VARCHAR(128) NOT NULL DEFAULT '',
    group_id INT(11) NOT NULL DEFAULT 0,
    KEY group_idx (group_id)
) ENGINE=MyISAM;

