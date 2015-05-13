INSERT INTO version (table_name, table_version) values ('carrierroute','3');
CREATE TABLE `carrierroute` (
    `id` INT(10) UNSIGNED AUTO_INCREMENT PRIMARY KEY NOT NULL,
    `carrier` INT(10) UNSIGNED DEFAULT 0 NOT NULL,
    `domain` INT(10) UNSIGNED DEFAULT 0 NOT NULL,
    `scan_prefix` VARCHAR(64) DEFAULT '' NOT NULL,
    `flags` INT(11) UNSIGNED DEFAULT 0 NOT NULL,
    `mask` INT(11) UNSIGNED DEFAULT 0 NOT NULL,
    `prob` FLOAT DEFAULT 0 NOT NULL,
    `strip` INT(11) UNSIGNED DEFAULT 0 NOT NULL,
    `rewrite_host` VARCHAR(128) DEFAULT '' NOT NULL,
    `rewrite_prefix` VARCHAR(64) DEFAULT '' NOT NULL,
    `rewrite_suffix` VARCHAR(64) DEFAULT '' NOT NULL,
    `description` VARCHAR(255) DEFAULT NULL
);

INSERT INTO version (table_name, table_version) values ('carrierfailureroute','2');
CREATE TABLE `carrierfailureroute` (
    `id` INT(10) UNSIGNED AUTO_INCREMENT PRIMARY KEY NOT NULL,
    `carrier` INT(10) UNSIGNED DEFAULT 0 NOT NULL,
    `domain` INT(10) UNSIGNED DEFAULT 0 NOT NULL,
    `scan_prefix` VARCHAR(64) DEFAULT '' NOT NULL,
    `host_name` VARCHAR(128) DEFAULT '' NOT NULL,
    `reply_code` VARCHAR(3) DEFAULT '' NOT NULL,
    `flags` INT(11) UNSIGNED DEFAULT 0 NOT NULL,
    `mask` INT(11) UNSIGNED DEFAULT 0 NOT NULL,
    `next_domain` INT(10) UNSIGNED DEFAULT 0 NOT NULL,
    `description` VARCHAR(255) DEFAULT NULL
);

INSERT INTO version (table_name, table_version) values ('carrier_name','1');
CREATE TABLE `carrier_name` (
    `id` INT(10) UNSIGNED AUTO_INCREMENT PRIMARY KEY NOT NULL,
    `carrier` VARCHAR(64) DEFAULT NULL
);

INSERT INTO version (table_name, table_version) values ('domain_name','1');
CREATE TABLE `domain_name` (
    `id` INT(10) UNSIGNED AUTO_INCREMENT PRIMARY KEY NOT NULL,
    `domain` VARCHAR(64) DEFAULT NULL
);

