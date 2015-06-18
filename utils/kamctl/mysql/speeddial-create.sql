INSERT INTO version (table_name, table_version) values ('speed_dial','2');
CREATE TABLE `speed_dial` (
    `id` INT(10) UNSIGNED AUTO_INCREMENT PRIMARY KEY NOT NULL,
    `username` VARCHAR(64) DEFAULT '' NOT NULL,
    `domain` VARCHAR(64) DEFAULT '' NOT NULL,
    `sd_username` VARCHAR(64) DEFAULT '' NOT NULL,
    `sd_domain` VARCHAR(64) DEFAULT '' NOT NULL,
    `new_uri` VARCHAR(128) DEFAULT '' NOT NULL,
    `fname` VARCHAR(64) DEFAULT '' NOT NULL,
    `lname` VARCHAR(64) DEFAULT '' NOT NULL,
    `description` VARCHAR(64) DEFAULT '' NOT NULL,
    CONSTRAINT speed_dial_idx UNIQUE (`username`, `domain`, `sd_domain`, `sd_username`)
);

