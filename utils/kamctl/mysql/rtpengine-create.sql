CREATE TABLE `rtpengine` (
    `id` INT(10) UNSIGNED AUTO_INCREMENT PRIMARY KEY NOT NULL,
    `setid` INT(10) UNSIGNED DEFAULT 0 NOT NULL,
    `url` VARCHAR(64) NOT NULL,
    `weight` INT(10) UNSIGNED DEFAULT 1 NOT NULL,
    `disabled` INT(1) DEFAULT 0 NOT NULL,
    `stamp` DATETIME DEFAULT '1900-01-01 00:00:01' NOT NULL,
    CONSTRAINT rtpengine_nodes UNIQUE (`setid`, `url`)
);

INSERT INTO version (table_name, table_version) values ('rtpengine','1');

