CREATE TABLE `ro_session` (
    `id` INT(10) UNSIGNED AUTO_INCREMENT PRIMARY KEY NOT NULL,
    `hash_entry` INT(10) UNSIGNED NOT NULL,
    `hash_id` INT(10) UNSIGNED NOT NULL,
    `session_id` VARCHAR(100) NOT NULL,
    `dlg_hash_entry` INT(10) UNSIGNED NOT NULL,
    `dlg_hash_id` INT(10) UNSIGNED NOT NULL,
    `direction` INT(1) UNSIGNED NOT NULL,
    `asserted_identity` VARCHAR(100) NOT NULL,
    `callee` VARCHAR(100) NOT NULL,
    `start_time` DATETIME DEFAULT NULL,
    `last_event_timestamp` DATETIME DEFAULT NULL,
    `reserved_secs` INT(10) DEFAULT NULL,
    `valid_for` INT(10) DEFAULT NULL,
    `state` INT(1) DEFAULT NULL,
    `incoming_trunk_id` VARCHAR(20) DEFAULT NULL,
    `outgoing_trunk_id` VARCHAR(20) DEFAULT NULL,
    `rating_group` INT(11) DEFAULT NULL,
    `service_identifier` INT(11) DEFAULT NULL,
    `auth_app_id` INT(11) NOT NULL,
    `auth_session_type` INT(11) NOT NULL,
    `pani` VARCHAR(100) DEFAULT NULL,
    `mac` VARCHAR(17) DEFAULT NULL,
    `app_provided_party` VARCHAR(100) DEFAULT NULL,
    `is_final_allocation` INT(1) UNSIGNED NOT NULL,
    `origin_host` VARCHAR(150) DEFAULT NULL
);

CREATE INDEX hash_idx ON ro_session (`hash_entry`, `hash_id`);

INSERT INTO version (table_name, table_version) values ('ro_session','3');

