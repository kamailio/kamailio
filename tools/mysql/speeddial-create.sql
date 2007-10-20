INSERT INTO version (table_name, table_version) values ('speed_dial','2');
CREATE TABLE speed_dial (
    id INT(10) UNSIGNED AUTO_INCREMENT PRIMARY KEY NOT NULL,
    username VARCHAR(64) NOT NULL DEFAULT '',
    domain VARCHAR(64) NOT NULL DEFAULT '',
    sd_username VARCHAR(64) NOT NULL DEFAULT '',
    sd_domain VARCHAR(64) NOT NULL DEFAULT '',
    new_uri VARCHAR(128) NOT NULL DEFAULT '',
    fname VARCHAR(64) NOT NULL DEFAULT '',
    lname VARCHAR(64) NOT NULL DEFAULT '',
    description VARCHAR(64) NOT NULL DEFAULT '',
    UNIQUE KEY speed_dial_idx (username, domain, sd_domain, sd_username)
) ENGINE=MyISAM;

