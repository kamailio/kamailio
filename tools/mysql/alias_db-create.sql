INSERT INTO version (table_name, table_version) values ('dbaliases','1');
CREATE TABLE dbaliases (
    id INT(10) UNSIGNED AUTO_INCREMENT PRIMARY KEY NOT NULL,
    alias_username VARCHAR(64) NOT NULL DEFAULT '',
    alias_domain VARCHAR(64) NOT NULL DEFAULT '',
    username VARCHAR(64) NOT NULL DEFAULT '',
    domain VARCHAR(64) NOT NULL DEFAULT '',
    UNIQUE KEY alias_key (alias_username, alias_domain),
    KEY alias_user (username, domain)
) ENGINE=MyISAM;

