INSERT INTO version (table_name, table_version) values ('uri','1');
CREATE TABLE uri (
    id INT(10) UNSIGNED AUTO_INCREMENT PRIMARY KEY NOT NULL,
    username VARCHAR(64) NOT NULL DEFAULT '',
    domain VARCHAR(64) NOT NULL DEFAULT '',
    uri_user VARCHAR(64) NOT NULL DEFAULT '',
    last_modified DATETIME NOT NULL DEFAULT '1900-01-01 00:00:01',
    UNIQUE KEY account_idx (username, domain, uri_user)
) ENGINE=MyISAM;

