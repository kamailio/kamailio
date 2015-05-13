INSERT INTO version (table_name, table_version) values ('rtpproxy','1');
CREATE TABLE `rtpproxy` (
    `id` INT(10) UNSIGNED AUTO_INCREMENT PRIMARY KEY NOT NULL,
    `setid` VARCHAR(32) DEFAULT 00 NOT NULL,
    `url` VARCHAR(64) DEFAULT '' NOT NULL,
    `flags` INT DEFAULT 0 NOT NULL,
    `weight` INT DEFAULT 1 NOT NULL,
    `description` VARCHAR(64) DEFAULT '' NOT NULL
);

