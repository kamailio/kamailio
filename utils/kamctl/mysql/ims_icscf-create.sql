CREATE TABLE `nds_trusted_domains` (
    `id` INT(10) AUTO_INCREMENT PRIMARY KEY NOT NULL,
    `trusted_domain` VARCHAR(83) DEFAULT '' NOT NULL
);

INSERT INTO version (table_name, table_version) values ('nds_trusted_domains','1');

CREATE TABLE `s_cscf` (
    `id` INT(10) AUTO_INCREMENT PRIMARY KEY NOT NULL,
    `name` VARCHAR(83) DEFAULT '' NOT NULL,
    `s_cscf_uri` VARCHAR(83) DEFAULT '' NOT NULL
);

INSERT INTO version (table_name, table_version) values ('s_cscf','1');

CREATE TABLE `s_cscf_capabilities` (
    `id` INT(10) AUTO_INCREMENT PRIMARY KEY NOT NULL,
    `id_s_cscf` INT(11) DEFAULT 0 NOT NULL,
    `capability` INT(11) DEFAULT 0 NOT NULL
);

CREATE INDEX idx_capability ON s_cscf_capabilities (`capability`);
CREATE INDEX idx_id_s_cscf ON s_cscf_capabilities (`id_s_cscf`);

INSERT INTO version (table_name, table_version) values ('s_cscf_capabilities','1');
