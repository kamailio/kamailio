INSERT INTO version (table_name, table_version) values ('userblacklist','1');
CREATE TABLE userblacklist (
    id INT(10) UNSIGNED AUTO_INCREMENT PRIMARY KEY NOT NULL,
    username VARCHAR(64) NOT NULL DEFAULT '',
    domain VARCHAR(64) NOT NULL DEFAULT '',
    prefix VARCHAR(64) NOT NULL DEFAULT '',
    whitelist INT(1) UNSIGNED NOT NULL DEFAULT 0,
    comment VARCHAR(64) NOT NULL DEFAULT '',
    KEY userblacklist_idx (username, domain, prefix)
) ENGINE=MyISAM;

INSERT INTO version (table_name, table_version) values ('globalblacklist','1');
CREATE TABLE globalblacklist (
    id INT(10) UNSIGNED AUTO_INCREMENT PRIMARY KEY NOT NULL,
    prefix VARCHAR(64) NOT NULL DEFAULT '',
    whitelist INT(1) UNSIGNED NOT NULL DEFAULT 0,
    comment VARCHAR(64) NOT NULL DEFAULT '',
    KEY userblacklist_idx (prefix)
) ENGINE=MyISAM;

