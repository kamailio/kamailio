INSERT INTO version (table_name, table_version) values ('uid_user_attrs','3');
CREATE TABLE `uid_user_attrs` (
    `id` INT(10) UNSIGNED AUTO_INCREMENT PRIMARY KEY NOT NULL,
    `uid` VARCHAR(64) NOT NULL,
    `name` VARCHAR(32) NOT NULL,
    `value` VARCHAR(128),
    `type` INT DEFAULT 0 NOT NULL,
    `flags` INT UNSIGNED DEFAULT 0 NOT NULL,
    CONSTRAINT userattrs_idx UNIQUE (`uid`, `name`, `value`)
);

