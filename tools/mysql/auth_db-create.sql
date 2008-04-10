INSERT INTO version (table_name, table_version) values ('subscriber','6');
CREATE TABLE subscriber (
    id INT(10) UNSIGNED AUTO_INCREMENT PRIMARY KEY NOT NULL,
    username VARCHAR(64) DEFAULT '' NOT NULL,
    domain VARCHAR(64) DEFAULT '' NOT NULL,
    password VARCHAR(25) DEFAULT '' NOT NULL,
    email_address VARCHAR(64) DEFAULT '' NOT NULL,
    ha1 VARCHAR(64) DEFAULT '' NOT NULL,
    ha1b VARCHAR(64) DEFAULT '' NOT NULL,
    rpid VARCHAR(64) DEFAULT NULL,
    UNIQUE KEY account_idx (username, domain),
    KEY username_idx (username)
) ENGINE=MyISAM;

