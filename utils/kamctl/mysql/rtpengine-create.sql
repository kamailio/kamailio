CREATE TABLE `rtpengine` (
    `setid` INT(10) UNSIGNED DEFAULT 0 NOT NULL,
    `url` VARCHAR(64) NOT NULL,
    `weight` INT(10) UNSIGNED DEFAULT 1 NOT NULL,
    `disabled` INT(1) DEFAULT 0 NOT NULL,
    CONSTRAINT rtpengine_nodes PRIMARY KEY  (`setid`, `url`)
);

INSERT INTO version (table_name, table_version) values ('rtpengine','1');

