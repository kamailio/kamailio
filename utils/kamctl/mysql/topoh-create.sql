CREATE TABLE `topoh_address` (
    `id` INT(10) UNSIGNED AUTO_INCREMENT PRIMARY KEY NOT NULL,
    `trust` INT(1) UNSIGNED DEFAULT 1 NOT NULL,
    `ip_addr` VARCHAR(50) NOT NULL,
    `mask` INT DEFAULT 32 NOT NULL,
    `port` SMALLINT(5) UNSIGNED DEFAULT 0 NOT NULL,
    `tag` VARCHAR(64)
);

INSERT INTO version (table_name, table_version) values ('topoh_address','1');

