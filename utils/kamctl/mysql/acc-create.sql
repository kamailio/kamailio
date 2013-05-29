INSERT INTO version (table_name, table_version) values ('acc','4');
CREATE TABLE acc (
    id INT(10) UNSIGNED AUTO_INCREMENT PRIMARY KEY NOT NULL,
    method VARCHAR(16) DEFAULT '' NOT NULL,
    from_tag VARCHAR(64) DEFAULT '' NOT NULL,
    to_tag VARCHAR(64) DEFAULT '' NOT NULL,
    callid VARCHAR(255) DEFAULT '' NOT NULL,
    sip_code VARCHAR(3) DEFAULT '' NOT NULL,
    sip_reason VARCHAR(32) DEFAULT '' NOT NULL,
    time DATETIME NOT NULL
) ENGINE=InnoDB;

CREATE INDEX callid_idx ON acc (callid);

INSERT INTO version (table_name, table_version) values ('missed_calls','3');
CREATE TABLE missed_calls (
    id INT(10) UNSIGNED AUTO_INCREMENT PRIMARY KEY NOT NULL,
    method VARCHAR(16) DEFAULT '' NOT NULL,
    from_tag VARCHAR(64) DEFAULT '' NOT NULL,
    to_tag VARCHAR(64) DEFAULT '' NOT NULL,
    callid VARCHAR(255) DEFAULT '' NOT NULL,
    sip_code VARCHAR(3) DEFAULT '' NOT NULL,
    sip_reason VARCHAR(32) DEFAULT '' NOT NULL,
    time DATETIME NOT NULL
) ENGINE=InnoDB;

CREATE INDEX callid_idx ON missed_calls (callid);

