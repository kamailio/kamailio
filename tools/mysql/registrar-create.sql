INSERT INTO version (table_name, table_version) values ('aliases','1004');
CREATE TABLE aliases (
    id INT(10) UNSIGNED AUTO_INCREMENT PRIMARY KEY NOT NULL,
    username VARCHAR(64) NOT NULL DEFAULT '',
    domain VARCHAR(64) DEFAULT NULL,
    contact VARCHAR(255) NOT NULL DEFAULT '',
    received VARCHAR(128) DEFAULT NULL,
    path VARCHAR(128) DEFAULT NULL,
    expires DATETIME NOT NULL DEFAULT '2020-05-28 21:32:15',
    q FLOAT(10,2) NOT NULL DEFAULT 1.0,
    callid VARCHAR(255) NOT NULL DEFAULT 'Default-Call-ID',
    cseq INT(11) NOT NULL DEFAULT 13,
    last_modified DATETIME NOT NULL DEFAULT '1900-01-01 00:00:01',
    flags INT(11) NOT NULL DEFAULT 0,
    cflags INT(11) NOT NULL DEFAULT 0,
    user_agent VARCHAR(255) NOT NULL DEFAULT '',
    socket VARCHAR(64) DEFAULT NULL,
    methods INT(11) DEFAULT NULL,
    KEY alias_idx (username, domain, contact)
) ENGINE=MyISAM;

