CREATE TABLE `secfilter` (
  `id` INT(10) UNSIGNED AUTO_INCREMENT PRIMARY KEY NOT NULL,
  `action` TINYINT(1) DEFAULT 0 NOT NULL,
  `type` TINYINT(1) DEFAULT 0 NOT NULL,
  `data` VARCHAR(64) DEFAULT '' NOT NULL
);

CREATE INDEX secfilter_idx ON secfilter (`action`, `type`, `data`);

INSERT INTO version (`table_name`, `table_version`) values ('secfilter', '1');

