INSERT INTO version (table_name, table_version) values ('rls_presentity','1');
CREATE TABLE `rls_presentity` (
    `id` INT(10) UNSIGNED AUTO_INCREMENT PRIMARY KEY NOT NULL,
    `rlsubs_did` VARCHAR(255) NOT NULL,
    `resource_uri` VARCHAR(128) NOT NULL,
    `content_type` VARCHAR(255) NOT NULL,
    `presence_state` BLOB NOT NULL,
    `expires` INT(11) NOT NULL,
    `updated` INT(11) NOT NULL,
    `auth_state` INT(11) NOT NULL,
    `reason` VARCHAR(64) NOT NULL,
    CONSTRAINT rls_presentity_idx UNIQUE (`rlsubs_did`, `resource_uri`)
);

CREATE INDEX rlsubs_idx ON rls_presentity (`rlsubs_did`);
CREATE INDEX updated_idx ON rls_presentity (`updated`);
CREATE INDEX expires_idx ON rls_presentity (`expires`);

INSERT INTO version (table_name, table_version) values ('rls_watchers','3');
CREATE TABLE `rls_watchers` (
    `id` INT(10) UNSIGNED AUTO_INCREMENT PRIMARY KEY NOT NULL,
    `presentity_uri` VARCHAR(128) NOT NULL,
    `to_user` VARCHAR(64) NOT NULL,
    `to_domain` VARCHAR(64) NOT NULL,
    `watcher_username` VARCHAR(64) NOT NULL,
    `watcher_domain` VARCHAR(64) NOT NULL,
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
    CONSTRAINT rls_watcher_idx UNIQUE (`callid`, `to_tag`, `from_tag`)
);

CREATE INDEX rls_watchers_update ON rls_watchers (`watcher_username`, `watcher_domain`, `event`);
CREATE INDEX rls_watchers_expires ON rls_watchers (`expires`);
CREATE INDEX updated_idx ON rls_watchers (`updated`);

