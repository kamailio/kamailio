INSERT INTO version (table_name, table_version) values ('domain','2');
CREATE TABLE `domain` (
    `id` INT(10) UNSIGNED AUTO_INCREMENT PRIMARY KEY NOT NULL,
    `domain` VARCHAR(64) NOT NULL,
    `did` VARCHAR(64) DEFAULT NULL,
    `last_modified` DATETIME DEFAULT '1900-01-01 00:00:01' NOT NULL,
    CONSTRAINT domain_idx UNIQUE (`domain`)
);

INSERT INTO version (table_name, table_version) values ('domain_attrs','1');
CREATE TABLE `domain_attrs` (
    `id` INT(10) UNSIGNED AUTO_INCREMENT PRIMARY KEY NOT NULL,
    `did` VARCHAR(64) NOT NULL,
    `name` VARCHAR(32) NOT NULL,
    `type` INT UNSIGNED NOT NULL,
    `value` VARCHAR(255) NOT NULL,
    `last_modified` DATETIME DEFAULT '1900-01-01 00:00:01' NOT NULL,
    CONSTRAINT domain_attrs_idx UNIQUE (`did`, `name`, `value`)
);

