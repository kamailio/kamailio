INSERT INTO version (table_name, table_version) values ('mohqcalls','1');
CREATE TABLE `mohqcalls` (
    `id` INT(10) UNSIGNED AUTO_INCREMENT PRIMARY KEY NOT NULL,
    `mohq_id` INT(10) UNSIGNED NOT NULL,
    `call_id` VARCHAR(100) NOT NULL,
    `call_status` INT UNSIGNED NOT NULL,
    `call_from` VARCHAR(100) NOT NULL,
    `call_contact` VARCHAR(100),
    `call_time` DATETIME NOT NULL,
    CONSTRAINT mohqcalls_idx UNIQUE (`call_id`)
);

INSERT INTO version (table_name, table_version) values ('mohqueues','1');
CREATE TABLE `mohqueues` (
    `id` INT(10) UNSIGNED AUTO_INCREMENT PRIMARY KEY NOT NULL,
    `name` VARCHAR(25) NOT NULL,
    `uri` VARCHAR(100) NOT NULL,
    `mohdir` VARCHAR(100),
    `mohfile` VARCHAR(100) NOT NULL,
    `debug` INT NOT NULL,
    CONSTRAINT mohqueue_uri_idx UNIQUE (`uri`),
    CONSTRAINT mohqueue_name_idx UNIQUE (`name`)
);

