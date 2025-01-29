CREATE TABLE `contact` (
    `id` INT(10) AUTO_INCREMENT PRIMARY KEY NOT NULL,
    `contact` VARCHAR(255) NOT NULL,
    `params` VARCHAR(255) DEFAULT NULL,
    `path` VARCHAR(255) DEFAULT NULL,
    `received` VARCHAR(255) DEFAULT NULL,
    `user_agent` VARCHAR(255) DEFAULT NULL,
    `expires` DATETIME DEFAULT NULL,
    `callid` VARCHAR(255) DEFAULT NULL,
    CONSTRAINT contact UNIQUE (`contact`)
);

INSERT INTO version (table_name, table_version) values ('contact','6');

CREATE TABLE `impu` (
    `id` INT(10) AUTO_INCREMENT PRIMARY KEY NOT NULL,
    `impu` VARCHAR(64) NOT NULL,
    `barring` INT(1) DEFAULT 0,
    `reg_state` INT(11) DEFAULT 0,
    `ccf1` VARCHAR(64) DEFAULT NULL,
    `ccf2` VARCHAR(64) DEFAULT NULL,
    `ecf1` VARCHAR(64) DEFAULT NULL,
    `ecf2` VARCHAR(64) DEFAULT NULL,
    `ims_subscription_data` BLOB,
    CONSTRAINT impu UNIQUE (`impu`)
);

INSERT INTO version (table_name, table_version) values ('impu','6');

CREATE TABLE `impu_contact` (
    `id` INT(10) AUTO_INCREMENT PRIMARY KEY NOT NULL,
    `impu_id` INT(10) NOT NULL,
    `contact_id` INT(10) NOT NULL,
    CONSTRAINT impu_id UNIQUE (`impu_id`, `contact_id`)
);

INSERT INTO version (table_name, table_version) values ('impu_contact','6');

CREATE TABLE `subscriber_scscf` (
    `id` INT(10) AUTO_INCREMENT PRIMARY KEY NOT NULL,
    `watcher_uri` VARCHAR(100) NOT NULL,
    `watcher_contact` VARCHAR(100) NOT NULL,
    `presentity_uri` VARCHAR(100) NOT NULL,
    `event` INT(11) NOT NULL,
    `expires` DATETIME NOT NULL,
    `version` INT(11) NOT NULL,
    `local_cseq` INT(11) NOT NULL,
    `call_id` VARCHAR(50) NOT NULL,
    `from_tag` VARCHAR(50) NOT NULL,
    `to_tag` VARCHAR(50) NOT NULL,
    `record_route` TEXT NOT NULL,
    `sockinfo_str` VARCHAR(50) NOT NULL,
    CONSTRAINT contact UNIQUE (`event`, `watcher_contact`, `presentity_uri`)
);

INSERT INTO version (table_name, table_version) values ('subscriber_scscf','6');

CREATE TABLE `impu_subscriber` (
    `id` INT(10) AUTO_INCREMENT PRIMARY KEY NOT NULL,
    `impu_id` INT(10) NOT NULL,
    `subscriber_id` INT(10) NOT NULL,
    CONSTRAINT impu_id UNIQUE (`impu_id`, `subscriber_id`)
);

INSERT INTO version (table_name, table_version) values ('impu_subscriber','6');
