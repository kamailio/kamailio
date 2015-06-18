INSERT INTO version (table_name, table_version) values ('presentity','4');
CREATE TABLE `presentity` (
    `id` INT(10) UNSIGNED AUTO_INCREMENT PRIMARY KEY NOT NULL,
    `username` VARCHAR(64) NOT NULL,
    `domain` VARCHAR(64) NOT NULL,
    `event` VARCHAR(64) NOT NULL,
    `etag` VARCHAR(64) NOT NULL,
    `expires` INT(11) NOT NULL,
    `received_time` INT(11) NOT NULL,
    `body` BLOB NOT NULL,
    `sender` VARCHAR(128) NOT NULL,
    `priority` INT(11) DEFAULT 0 NOT NULL,
    CONSTRAINT presentity_idx UNIQUE (`username`, `domain`, `event`, `etag`)
);

CREATE INDEX presentity_expires ON presentity (`expires`);
CREATE INDEX account_idx ON presentity (`username`, `domain`, `event`);

INSERT INTO version (table_name, table_version) values ('active_watchers','11');
CREATE TABLE `active_watchers` (
    `id` INT(10) UNSIGNED AUTO_INCREMENT PRIMARY KEY NOT NULL,
    `presentity_uri` VARCHAR(128) NOT NULL,
    `watcher_username` VARCHAR(64) NOT NULL,
    `watcher_domain` VARCHAR(64) NOT NULL,
    `to_user` VARCHAR(64) NOT NULL,
    `to_domain` VARCHAR(64) NOT NULL,
    `event` VARCHAR(64) DEFAULT 'presence' NOT NULL,
    `event_id` VARCHAR(64),
    `to_tag` VARCHAR(64) NOT NULL,
    `from_tag` VARCHAR(64) NOT NULL,
    `callid` VARCHAR(255) NOT NULL,
    `local_cseq` INT(11) NOT NULL,
    `remote_cseq` INT(11) NOT NULL,
    `contact` VARCHAR(128) NOT NULL,
    `record_route` TEXT,
    `expires` INT(11) NOT NULL,
    `status` INT(11) DEFAULT 2 NOT NULL,
    `reason` VARCHAR(64) NOT NULL,
    `version` INT(11) DEFAULT 0 NOT NULL,
    `socket_info` VARCHAR(64) NOT NULL,
    `local_contact` VARCHAR(128) NOT NULL,
    `from_user` VARCHAR(64) NOT NULL,
    `from_domain` VARCHAR(64) NOT NULL,
    `updated` INT(11) NOT NULL,
    `updated_winfo` INT(11) NOT NULL,
    CONSTRAINT active_watchers_idx UNIQUE (`callid`, `to_tag`, `from_tag`)
);

CREATE INDEX active_watchers_expires ON active_watchers (`expires`);
CREATE INDEX active_watchers_pres ON active_watchers (`presentity_uri`, `event`);
CREATE INDEX updated_idx ON active_watchers (`updated`);
CREATE INDEX updated_winfo_idx ON active_watchers (`updated_winfo`, `presentity_uri`);

INSERT INTO version (table_name, table_version) values ('watchers','3');
CREATE TABLE `watchers` (
    `id` INT(10) UNSIGNED AUTO_INCREMENT PRIMARY KEY NOT NULL,
    `presentity_uri` VARCHAR(128) NOT NULL,
    `watcher_username` VARCHAR(64) NOT NULL,
    `watcher_domain` VARCHAR(64) NOT NULL,
    `event` VARCHAR(64) DEFAULT 'presence' NOT NULL,
    `status` INT(11) NOT NULL,
    `reason` VARCHAR(64),
    `inserted_time` INT(11) NOT NULL,
    CONSTRAINT watcher_idx UNIQUE (`presentity_uri`, `watcher_username`, `watcher_domain`, `event`)
);

INSERT INTO version (table_name, table_version) values ('xcap','4');
CREATE TABLE `xcap` (
    `id` INT(10) UNSIGNED AUTO_INCREMENT PRIMARY KEY NOT NULL,
    `username` VARCHAR(64) NOT NULL,
    `domain` VARCHAR(64) NOT NULL,
    `doc` MEDIUMBLOB NOT NULL,
    `doc_type` INT(11) NOT NULL,
    `etag` VARCHAR(64) NOT NULL,
    `source` INT(11) NOT NULL,
    `doc_uri` VARCHAR(255) NOT NULL,
    `port` INT(11) NOT NULL,
    CONSTRAINT doc_uri_idx UNIQUE (`doc_uri`)
);

CREATE INDEX account_doc_type_idx ON xcap (`username`, `domain`, `doc_type`);
CREATE INDEX account_doc_type_uri_idx ON xcap (`username`, `domain`, `doc_type`, `doc_uri`);
CREATE INDEX account_doc_uri_idx ON xcap (`username`, `domain`, `doc_uri`);

INSERT INTO version (table_name, table_version) values ('pua','7');
CREATE TABLE `pua` (
    `id` INT(10) UNSIGNED AUTO_INCREMENT PRIMARY KEY NOT NULL,
    `pres_uri` VARCHAR(128) NOT NULL,
    `pres_id` VARCHAR(255) NOT NULL,
    `event` INT(11) NOT NULL,
    `expires` INT(11) NOT NULL,
    `desired_expires` INT(11) NOT NULL,
    `flag` INT(11) NOT NULL,
    `etag` VARCHAR(64) NOT NULL,
    `tuple_id` VARCHAR(64),
    `watcher_uri` VARCHAR(128) NOT NULL,
    `call_id` VARCHAR(255) NOT NULL,
    `to_tag` VARCHAR(64) NOT NULL,
    `from_tag` VARCHAR(64) NOT NULL,
    `cseq` INT(11) NOT NULL,
    `record_route` TEXT,
    `contact` VARCHAR(128) NOT NULL,
    `remote_contact` VARCHAR(128) NOT NULL,
    `version` INT(11) NOT NULL,
    `extra_headers` TEXT NOT NULL,
    CONSTRAINT pua_idx UNIQUE (`etag`, `tuple_id`, `call_id`, `from_tag`)
);

CREATE INDEX expires_idx ON pua (`expires`);
CREATE INDEX dialog1_idx ON pua (`pres_id`, `pres_uri`);
CREATE INDEX dialog2_idx ON pua (`call_id`, `from_tag`);
CREATE INDEX record_idx ON pua (`pres_id`);

