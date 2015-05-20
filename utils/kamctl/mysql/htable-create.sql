INSERT INTO version (table_name, table_version) values ('htable','2');
CREATE TABLE `htable` (
    `id` INT(10) UNSIGNED AUTO_INCREMENT PRIMARY KEY NOT NULL,
    `key_name` VARCHAR(64) DEFAULT '' NOT NULL,
    `key_type` INT DEFAULT 0 NOT NULL,
    `value_type` INT DEFAULT 0 NOT NULL,
    `key_value` VARCHAR(128) DEFAULT '' NOT NULL,
    `expires` INT DEFAULT 0 NOT NULL
);

