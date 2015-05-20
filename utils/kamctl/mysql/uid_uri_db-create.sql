INSERT INTO version (table_name, table_version) values ('uid_uri','3');
CREATE TABLE `uid_uri` (
    `id` INT(10) UNSIGNED AUTO_INCREMENT PRIMARY KEY NOT NULL,
    `uid` VARCHAR(64) NOT NULL,
    `did` VARCHAR(64) NOT NULL,
    `username` VARCHAR(64) NOT NULL,
    `flags` INT UNSIGNED DEFAULT 0 NOT NULL,
    `scheme` VARCHAR(8) DEFAULT 'sip' NOT NULL
);

CREATE INDEX uri_idx1 ON uid_uri (`username`, `did`, `scheme`);
CREATE INDEX uri_uid ON uid_uri (`uid`);

INSERT INTO version (table_name, table_version) values ('uid_uri_attrs','2');
CREATE TABLE `uid_uri_attrs` (
    `id` INT(10) UNSIGNED AUTO_INCREMENT PRIMARY KEY NOT NULL,
    `username` VARCHAR(64) NOT NULL,
    `did` VARCHAR(64) NOT NULL,
    `name` VARCHAR(32) NOT NULL,
    `value` VARCHAR(128),
    `type` INT DEFAULT 0 NOT NULL,
    `flags` INT UNSIGNED DEFAULT 0 NOT NULL,
    `scheme` VARCHAR(8) DEFAULT 'sip' NOT NULL,
    CONSTRAINT uriattrs_idx UNIQUE (`username`, `did`, `name`, `value`, `scheme`)
);

