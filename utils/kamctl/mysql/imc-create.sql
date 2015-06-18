INSERT INTO version (table_name, table_version) values ('imc_rooms','1');
CREATE TABLE `imc_rooms` (
    `id` INT(10) UNSIGNED AUTO_INCREMENT PRIMARY KEY NOT NULL,
    `name` VARCHAR(64) NOT NULL,
    `domain` VARCHAR(64) NOT NULL,
    `flag` INT(11) NOT NULL,
    CONSTRAINT name_domain_idx UNIQUE (`name`, `domain`)
);

INSERT INTO version (table_name, table_version) values ('imc_members','1');
CREATE TABLE `imc_members` (
    `id` INT(10) UNSIGNED AUTO_INCREMENT PRIMARY KEY NOT NULL,
    `username` VARCHAR(64) NOT NULL,
    `domain` VARCHAR(64) NOT NULL,
    `room` VARCHAR(64) NOT NULL,
    `flag` INT(11) NOT NULL,
    CONSTRAINT account_room_idx UNIQUE (`username`, `domain`, `room`)
);

