INSERT INTO version (table_name, table_version) values ('dbaliases','1');
CREATE TABLE dbaliases (
    id INT(10) UNSIGNED AUTO_INCREMENT PRIMARY KEY NOT NULL,
    alias_username VARCHAR(64) DEFAULT '' NOT NULL,
    alias_domain VARCHAR(64) DEFAULT '' NOT NULL,
    username VARCHAR(64) DEFAULT '' NOT NULL,
    domain VARCHAR(64) DEFAULT '' NOT NULL,
    UNIQUE KEY alias_idx (alias_username, alias_domain),
    KEY target_idx (username, domain)
) ENGINE=MyISAM;

