INSERT INTO version (table_name, table_version) values ('uid_global_attrs','1');
CREATE TABLE `uid_global_attrs` (
    `id` INT(10) UNSIGNED AUTO_INCREMENT PRIMARY KEY NOT NULL,
    `name` VARCHAR(32) NOT NULL,
    `type` INT DEFAULT 0 NOT NULL,
    `value` VARCHAR(128),
    `flags` INT UNSIGNED DEFAULT 0 NOT NULL,
    CONSTRAINT global_attrs_idx UNIQUE (`name`, `value`)
);

