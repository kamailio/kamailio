INSERT INTO version (table_name, table_version) values ('userblacklist','1');
CREATE TABLE `userblacklist` (
    `id` INT(10) UNSIGNED AUTO_INCREMENT PRIMARY KEY NOT NULL,
    `username` VARCHAR(64) DEFAULT '' NOT NULL,
    `domain` VARCHAR(64) DEFAULT '' NOT NULL,
    `prefix` VARCHAR(64) DEFAULT '' NOT NULL,
    `whitelist` TINYINT(1) DEFAULT 0 NOT NULL
);

CREATE INDEX userblacklist_idx ON userblacklist (`username`, `domain`, `prefix`);

INSERT INTO version (table_name, table_version) values ('globalblacklist','1');
CREATE TABLE `globalblacklist` (
    `id` INT(10) UNSIGNED AUTO_INCREMENT PRIMARY KEY NOT NULL,
    `prefix` VARCHAR(64) DEFAULT '' NOT NULL,
    `whitelist` TINYINT(1) DEFAULT 0 NOT NULL,
    `description` VARCHAR(255) DEFAULT NULL
);

CREATE INDEX globalblacklist_idx ON globalblacklist (`prefix`);

