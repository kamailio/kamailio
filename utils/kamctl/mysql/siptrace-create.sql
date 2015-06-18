INSERT INTO version (table_name, table_version) values ('sip_trace','4');
CREATE TABLE `sip_trace` (
    `id` INT(10) UNSIGNED AUTO_INCREMENT PRIMARY KEY NOT NULL,
    `time_stamp` DATETIME DEFAULT '1900-01-01 00:00:01' NOT NULL,
    `time_us` INT UNSIGNED DEFAULT 0 NOT NULL,
    `callid` VARCHAR(255) DEFAULT '' NOT NULL,
    `traced_user` VARCHAR(128) DEFAULT '' NOT NULL,
    `msg` MEDIUMTEXT NOT NULL,
    `method` VARCHAR(50) DEFAULT '' NOT NULL,
    `status` VARCHAR(128) DEFAULT '' NOT NULL,
    `fromip` VARCHAR(50) DEFAULT '' NOT NULL,
    `toip` VARCHAR(50) DEFAULT '' NOT NULL,
    `fromtag` VARCHAR(64) DEFAULT '' NOT NULL,
    `totag` VARCHAR(64) DEFAULT '' NOT NULL,
    `direction` VARCHAR(4) DEFAULT '' NOT NULL
);

CREATE INDEX traced_user_idx ON sip_trace (`traced_user`);
CREATE INDEX date_idx ON sip_trace (`time_stamp`);
CREATE INDEX fromip_idx ON sip_trace (`fromip`);
CREATE INDEX callid_idx ON sip_trace (`callid`);

