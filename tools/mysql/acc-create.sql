INSERT INTO version (table_name, table_version) values ('acc','4');
CREATE TABLE acc (
    id INT(10) UNSIGNED AUTO_INCREMENT PRIMARY KEY NOT NULL,
    method VARCHAR(16) DEFAULT '' NOT NULL,
    from_tag VARCHAR(64) DEFAULT '' NOT NULL,
    to_tag VARCHAR(64) DEFAULT '' NOT NULL,
    callid VARCHAR(64) DEFAULT '' NOT NULL,
    sip_code VARCHAR(3) DEFAULT '' NOT NULL,
    sip_reason VARCHAR(32) DEFAULT '' NOT NULL,
    time DATETIME NOT NULL,
    KEY callid_idx (callid)
) ENGINE=MyISAM;

INSERT INTO version (table_name, table_version) values ('missed_calls','3');
CREATE TABLE missed_calls (
    id INT(10) UNSIGNED AUTO_INCREMENT PRIMARY KEY NOT NULL,
    method VARCHAR(16) DEFAULT '' NOT NULL,
    from_tag VARCHAR(64) DEFAULT '' NOT NULL,
    to_tag VARCHAR(64) DEFAULT '' NOT NULL,
    callid VARCHAR(64) DEFAULT '' NOT NULL,
    sip_code VARCHAR(3) DEFAULT '' NOT NULL,
    sip_reason VARCHAR(32) DEFAULT '' NOT NULL,
    time DATETIME NOT NULL,
    KEY callid_idx (callid)
) ENGINE=MyISAM;

