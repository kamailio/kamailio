INSERT INTO version (table_name, table_version) values ('acc','4');
CREATE TABLE acc (
    id INT(10) UNSIGNED AUTO_INCREMENT PRIMARY KEY NOT NULL,
    method VARCHAR(16) NOT NULL DEFAULT '',
    from_tag VARCHAR(64) NOT NULL DEFAULT '',
    to_tag VARCHAR(64) NOT NULL DEFAULT '',
    callid VARCHAR(64) NOT NULL DEFAULT '',
    sip_code VARCHAR(3) NOT NULL DEFAULT '',
    sip_reason VARCHAR(32) NOT NULL DEFAULT '',
    time DATETIME NOT NULL,
    KEY callid_idx (callid)
) ENGINE=MyISAM;

INSERT INTO version (table_name, table_version) values ('missed_calls','3');
CREATE TABLE missed_calls (
    id INT(10) UNSIGNED AUTO_INCREMENT PRIMARY KEY NOT NULL,
    method VARCHAR(16) NOT NULL DEFAULT '',
    from_tag VARCHAR(64) NOT NULL DEFAULT '',
    to_tag VARCHAR(64) NOT NULL DEFAULT '',
    callid VARCHAR(64) NOT NULL DEFAULT '',
    sip_code VARCHAR(3) NOT NULL DEFAULT '',
    sip_reason VARCHAR(32) NOT NULL DEFAULT '',
    time DATETIME NOT NULL,
    KEY callid_idx (callid)
) ENGINE=MyISAM;

