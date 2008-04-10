INSERT INTO version (table_name, table_version) values ('userblacklist','1');
CREATE TABLE userblacklist (
    id INT(10) UNSIGNED AUTO_INCREMENT PRIMARY KEY NOT NULL,
    username VARCHAR(64) DEFAULT '' NOT NULL,
    domain VARCHAR(64) DEFAULT '' NOT NULL,
    prefix VARCHAR(64) DEFAULT '' NOT NULL,
    whitelist INT(1) UNSIGNED DEFAULT 0 NOT NULL,
    description VARCHAR(64) DEFAULT '' NOT NULL,
    KEY userblacklist_idx (username, domain, prefix)
) ENGINE=MyISAM;

INSERT INTO version (table_name, table_version) values ('globalblacklist','1');
CREATE TABLE globalblacklist (
    id INT(10) UNSIGNED AUTO_INCREMENT PRIMARY KEY NOT NULL,
    prefix VARCHAR(64) DEFAULT '' NOT NULL,
    whitelist INT(1) UNSIGNED DEFAULT 0 NOT NULL,
    description VARCHAR(64) DEFAULT '' NOT NULL,
    KEY globalblacklist_idx (prefix)
) ENGINE=MyISAM;

