INSERT INTO version (table_name, table_version) values ('dbaliases','1');
CREATE TABLE `dbaliases` (
    `id` INT(10) UNSIGNED AUTO_INCREMENT PRIMARY KEY NOT NULL,
    `alias_username` VARCHAR(64) DEFAULT '' NOT NULL,
    `alias_domain` VARCHAR(64) DEFAULT '' NOT NULL,
    `username` VARCHAR(64) DEFAULT '' NOT NULL,
    `domain` VARCHAR(64) DEFAULT '' NOT NULL
);

CREATE INDEX alias_user_idx ON dbaliases (`alias_username`);
CREATE INDEX alias_idx ON dbaliases (`alias_username`, `alias_domain`);
CREATE INDEX target_idx ON dbaliases (`username`, `domain`);

