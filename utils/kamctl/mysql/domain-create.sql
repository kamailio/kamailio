INSERT INTO version (table_name, table_version) values ('domain','1');
CREATE TABLE domain (
    id INT(10) UNSIGNED AUTO_INCREMENT PRIMARY KEY NOT NULL,
    domain VARCHAR(64) DEFAULT '' NOT NULL,
    last_modified DATETIME DEFAULT '1900-01-01 00:00:01' NOT NULL,
    CONSTRAINT domain_idx UNIQUE (domain)
) ENGINE=MyISAM;

