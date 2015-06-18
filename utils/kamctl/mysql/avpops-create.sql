INSERT INTO version (table_name, table_version) values ('usr_preferences','2');
CREATE TABLE `usr_preferences` (
    `id` INT(10) UNSIGNED AUTO_INCREMENT PRIMARY KEY NOT NULL,
    `uuid` VARCHAR(64) DEFAULT '' NOT NULL,
    `username` VARCHAR(128) DEFAULT 0 NOT NULL,
    `domain` VARCHAR(64) DEFAULT '' NOT NULL,
    `attribute` VARCHAR(32) DEFAULT '' NOT NULL,
    `type` INT(11) DEFAULT 0 NOT NULL,
    `value` VARCHAR(128) DEFAULT '' NOT NULL,
    `last_modified` DATETIME DEFAULT '1900-01-01 00:00:01' NOT NULL
);

CREATE INDEX ua_idx ON usr_preferences (`uuid`, `attribute`);
CREATE INDEX uda_idx ON usr_preferences (`username`, `domain`, `attribute`);

