CREATE TABLE `security` (
  `id` INT(10) UNSIGNED AUTO_INCREMENT PRIMARY KEY NOT NULL,
  `action` TINYINT(1) DEFAULT 0 NOT NULL,
  `type` TINYINT(1) DEFAULT 0 NOT NULL,
  `data` VARCHAR(64) DEFAULT '' NOT NULL
);

CREATE INDEX security_idx ON security (`action`, `type`, `data`);

INSERT INTO version (`table_name`, `table_version`) values ('security', '1');

