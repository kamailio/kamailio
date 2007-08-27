INSERT INTO version (table_name, table_version) values ('usr_preferences','2');
CREATE TABLE usr_preferences (
    id INT(10) UNSIGNED AUTO_INCREMENT PRIMARY KEY NOT NULL,
    uuid VARCHAR(64) NOT NULL DEFAULT '',
    username VARCHAR(128) NOT NULL DEFAULT 0,
    domain VARCHAR(64) NOT NULL DEFAULT '',
    attribute VARCHAR(32) NOT NULL DEFAULT '',
    type INT(11) NOT NULL DEFAULT 0,
    value VARCHAR(128) NOT NULL DEFAULT '',
    last_modified DATETIME NOT NULL DEFAULT '1900-01-01 00:00:01',
    KEY ua_idx (uuid, attribute),
    KEY uda_idx (username, domain, attribute)
) ENGINE=MyISAM;

