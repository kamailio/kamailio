CREATE TABLE `sca_subscriptions` (
    `id` INT(10) UNSIGNED AUTO_INCREMENT PRIMARY KEY NOT NULL,
    `subscriber` VARCHAR(255) NOT NULL,
    `aor` VARCHAR(255) NOT NULL,
    `event` INT DEFAULT 0 NOT NULL,
    `expires` INT(11) DEFAULT 0 NOT NULL,
    `state` INT DEFAULT 0 NOT NULL,
    `app_idx` INT DEFAULT 0 NOT NULL,
    `call_id` VARCHAR(255) NOT NULL,
    `from_tag` VARCHAR(128) NOT NULL,
    `to_tag` VARCHAR(128) NOT NULL,
    `record_route` TEXT,
    `notify_cseq` INT(11) NOT NULL,
    `subscribe_cseq` INT(11) NOT NULL,
    `server_id` INT(11) DEFAULT 0 NOT NULL,
    CONSTRAINT sca_subscriptions_idx UNIQUE (`subscriber`, `call_id`, `from_tag`, `to_tag`)
);

CREATE INDEX sca_expires_idx ON sca_subscriptions (`server_id`, `expires`);
CREATE INDEX sca_subscribers_idx ON sca_subscriptions (`subscriber`, `event`);

INSERT INTO version (table_name, table_version) values ('sca_subscriptions','2');

