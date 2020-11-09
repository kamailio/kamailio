CREATE TABLE `userblocklist` (
    `id` INT(10) UNSIGNED AUTO_INCREMENT PRIMARY KEY NOT NULL,
    `username` VARCHAR(64) DEFAULT '' NOT NULL,
    `domain` VARCHAR(64) DEFAULT '' NOT NULL,
    `prefix` VARCHAR(64) DEFAULT '' NOT NULL,
    `allowlist` TINYINT(1) DEFAULT 0 NOT NULL
);

CREATE INDEX userblocklist_idx ON userblocklist (`username`, `domain`, `prefix`);

INSERT INTO version (table_name, table_version) values ('userblocklist','1');

CREATE TABLE `globalblocklist` (
    `id` INT(10) UNSIGNED AUTO_INCREMENT PRIMARY KEY NOT NULL,
    `prefix` VARCHAR(64) DEFAULT '' NOT NULL,
    `allowlist` TINYINT(1) DEFAULT 0 NOT NULL,
    `description` VARCHAR(255) DEFAULT NULL
);

CREATE INDEX globalblocklist_idx ON globalblocklist (`prefix`);

INSERT INTO version (table_name, table_version) values ('globalblocklist','1');

