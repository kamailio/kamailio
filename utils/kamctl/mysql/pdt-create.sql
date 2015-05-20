INSERT INTO version (table_name, table_version) values ('pdt','1');
CREATE TABLE `pdt` (
    `id` INT(10) UNSIGNED AUTO_INCREMENT PRIMARY KEY NOT NULL,
    `sdomain` VARCHAR(128) NOT NULL,
    `prefix` VARCHAR(32) NOT NULL,
    `domain` VARCHAR(128) DEFAULT '' NOT NULL,
    CONSTRAINT sdomain_prefix_idx UNIQUE (`sdomain`, `prefix`)
);

