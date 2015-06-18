INSERT INTO version (table_name, table_version) values ('dispatcher','4');
CREATE TABLE `dispatcher` (
    `id` INT(10) UNSIGNED AUTO_INCREMENT PRIMARY KEY NOT NULL,
    `setid` INT DEFAULT 0 NOT NULL,
    `destination` VARCHAR(192) DEFAULT '' NOT NULL,
    `flags` INT DEFAULT 0 NOT NULL,
    `priority` INT DEFAULT 0 NOT NULL,
    `attrs` VARCHAR(128) DEFAULT '' NOT NULL,
    `description` VARCHAR(64) DEFAULT '' NOT NULL
);

