CREATE TABLE `version` (
    `table_name` VARCHAR(32) NOT NULL,
    `table_version` INT UNSIGNED DEFAULT 0 NOT NULL,
    CONSTRAINT table_name_idx UNIQUE (`table_name`)
);

INSERT INTO version (table_name, table_version) values ('version','1');

