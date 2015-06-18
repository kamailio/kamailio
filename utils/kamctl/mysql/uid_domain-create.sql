INSERT INTO version (table_name, table_version) values ('uid_domain','2');
CREATE TABLE `uid_domain` (
    `id` INT(10) UNSIGNED AUTO_INCREMENT PRIMARY KEY NOT NULL,
    `did` VARCHAR(64) NOT NULL,
    `domain` VARCHAR(64) NOT NULL,
    `flags` INT UNSIGNED DEFAULT 0 NOT NULL,
    CONSTRAINT domain_idx UNIQUE (`domain`)
);

CREATE INDEX did_idx ON uid_domain (`did`);

INSERT INTO version (table_name, table_version) values ('uid_domain_attrs','1');
CREATE TABLE `uid_domain_attrs` (
    `id` INT(10) UNSIGNED AUTO_INCREMENT PRIMARY KEY NOT NULL,
    `did` VARCHAR(64),
    `name` VARCHAR(32) NOT NULL,
    `type` INT DEFAULT 0 NOT NULL,
    `value` VARCHAR(128),
    `flags` INT UNSIGNED DEFAULT 0 NOT NULL,
    CONSTRAINT domain_attr_idx UNIQUE (`did`, `name`, `value`)
);

CREATE INDEX domain_did ON uid_domain_attrs (`did`, `flags`);

