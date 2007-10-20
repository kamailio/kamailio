INSERT INTO version (table_name, table_version) values ('domain','1');
CREATE TABLE domain (
    id INT(10) UNSIGNED AUTO_INCREMENT PRIMARY KEY NOT NULL,
    domain VARCHAR(64) NOT NULL DEFAULT '',
    last_modified DATETIME NOT NULL DEFAULT '1900-01-01 00:00:01',
    UNIQUE KEY domain_idx (domain)
) ENGINE=MyISAM;

