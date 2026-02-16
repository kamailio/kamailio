CREATE TABLE `dialog_in` (
    `id` INT(10) UNSIGNED AUTO_INCREMENT PRIMARY KEY NOT NULL,
    `hash_entry` INT(10) UNSIGNED NOT NULL,
    `hash_id` INT(10) UNSIGNED NOT NULL,
    `did` VARCHAR(45) NOT NULL,
    `callid` VARCHAR(255) NOT NULL,
    `from_uri` VARCHAR(255) NOT NULL,
    `from_tag` VARCHAR(128) NOT NULL,
    `caller_original_cseq` VARCHAR(20) NOT NULL,
    `req_uri` VARCHAR(255) NOT NULL,
    `caller_route_set` VARCHAR(512),
    `caller_contact` VARCHAR(255) NOT NULL,
    `caller_sock` VARCHAR(64) NOT NULL,
    `timeout` INT(10) UNSIGNED DEFAULT 0 NOT NULL,
    `state` INT(10) UNSIGNED NOT NULL,
    `start_time` INT(10) UNSIGNED NOT NULL,
    `sflags` INT(10) UNSIGNED DEFAULT 0 NOT NULL,
    `toroute_name` VARCHAR(32),
    `toroute_index` INT(10) UNSIGNED NOT NULL
);

CREATE INDEX hash_idx ON dialog_in (`hash_entry`, `hash_id`);

INSERT INTO version (table_name, table_version) values ('dialog_in','7');

CREATE TABLE `dialog_out` (
    `id` INT(10) UNSIGNED AUTO_INCREMENT PRIMARY KEY NOT NULL,
    `hash_entry` INT(10) UNSIGNED NOT NULL,
    `hash_id` INT(10) UNSIGNED NOT NULL,
    `did` VARCHAR(45) NOT NULL,
    `to_uri` VARCHAR(255) NOT NULL,
    `to_tag` VARCHAR(128) NOT NULL,
    `caller_cseq` VARCHAR(20) NOT NULL,
    `callee_cseq` VARCHAR(20) NOT NULL,
    `callee_contact` VARCHAR(255) NOT NULL,
    `callee_route_set` VARCHAR(512),
    `callee_sock` VARCHAR(64) NOT NULL
);

CREATE INDEX hash_idx ON dialog_out (`hash_entry`, `hash_id`);

INSERT INTO version (table_name, table_version) values ('dialog_out','7');

CREATE TABLE `dialog_vars` (
    `id` INT(10) UNSIGNED AUTO_INCREMENT PRIMARY KEY NOT NULL,
    `hash_entry` INT(10) UNSIGNED NOT NULL,
    `hash_id` INT(10) UNSIGNED NOT NULL,
    `dialog_key` VARCHAR(128) NOT NULL,
    `dialog_value` VARCHAR(512) NOT NULL
);

CREATE INDEX hash_idx ON dialog_vars (`hash_entry`, `hash_id`);

INSERT INTO version (table_name, table_version) values ('dialog_vars','1');

