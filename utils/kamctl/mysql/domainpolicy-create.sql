INSERT INTO version (table_name, table_version) values ('domainpolicy','2');
CREATE TABLE `domainpolicy` (
    `id` INT(10) UNSIGNED AUTO_INCREMENT PRIMARY KEY NOT NULL,
    `rule` VARCHAR(255) NOT NULL,
    `type` VARCHAR(255) NOT NULL,
    `att` VARCHAR(255),
    `val` VARCHAR(128),
    `description` VARCHAR(255) NOT NULL,
    CONSTRAINT rav_idx UNIQUE (`rule`, `att`, `val`)
);

CREATE INDEX rule_idx ON domainpolicy (`rule`);

