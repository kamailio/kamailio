INSERT INTO version (table_name, table_version) values ('uacreg','1');
CREATE TABLE `uacreg` (
    `id` INT(10) UNSIGNED AUTO_INCREMENT PRIMARY KEY NOT NULL,
    `l_uuid` VARCHAR(64) DEFAULT '' NOT NULL,
    `l_username` VARCHAR(64) DEFAULT '' NOT NULL,
    `l_domain` VARCHAR(128) DEFAULT '' NOT NULL,
    `r_username` VARCHAR(64) DEFAULT '' NOT NULL,
    `r_domain` VARCHAR(128) DEFAULT '' NOT NULL,
    `realm` VARCHAR(64) DEFAULT '' NOT NULL,
    `auth_username` VARCHAR(64) DEFAULT '' NOT NULL,
    `auth_password` VARCHAR(64) DEFAULT '' NOT NULL,
    `auth_proxy` VARCHAR(64) DEFAULT '' NOT NULL,
    `expires` INT DEFAULT 0 NOT NULL,
    CONSTRAINT l_uuid_idx UNIQUE (`l_uuid`)
);

