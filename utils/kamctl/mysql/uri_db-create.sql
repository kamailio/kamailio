INSERT INTO version (table_name, table_version) values ('uri','1');
CREATE TABLE `uri` (
    `id` INT(10) UNSIGNED AUTO_INCREMENT PRIMARY KEY NOT NULL,
    `username` VARCHAR(64) DEFAULT '' NOT NULL,
    `domain` VARCHAR(64) DEFAULT '' NOT NULL,
    `uri_user` VARCHAR(64) DEFAULT '' NOT NULL,
    `last_modified` DATETIME DEFAULT '1900-01-01 00:00:01' NOT NULL,
    CONSTRAINT account_idx UNIQUE (`username`, `domain`, `uri_user`)
);

