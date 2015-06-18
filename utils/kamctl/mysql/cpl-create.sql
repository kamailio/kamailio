INSERT INTO version (table_name, table_version) values ('cpl','1');
CREATE TABLE `cpl` (
    `id` INT(10) UNSIGNED AUTO_INCREMENT PRIMARY KEY NOT NULL,
    `username` VARCHAR(64) NOT NULL,
    `domain` VARCHAR(64) DEFAULT '' NOT NULL,
    `cpl_xml` TEXT,
    `cpl_bin` TEXT,
    CONSTRAINT account_idx UNIQUE (`username`, `domain`)
);

