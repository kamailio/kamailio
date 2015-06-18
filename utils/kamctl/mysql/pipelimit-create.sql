INSERT INTO version (table_name, table_version) values ('pl_pipes','1');
CREATE TABLE `pl_pipes` (
    `id` INT(10) UNSIGNED AUTO_INCREMENT PRIMARY KEY NOT NULL,
    `pipeid` VARCHAR(64) DEFAULT '' NOT NULL,
    `algorithm` VARCHAR(32) DEFAULT '' NOT NULL,
    `plimit` INT DEFAULT 0 NOT NULL
);

