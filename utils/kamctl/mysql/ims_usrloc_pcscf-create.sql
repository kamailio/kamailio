CREATE TABLE `pcscf_location` (
    `id` INT(10) UNSIGNED AUTO_INCREMENT PRIMARY KEY NOT NULL,
    `domain` VARCHAR(64) NOT NULL,
    `aor` VARCHAR(255) NOT NULL,
    `host` VARCHAR(100) NOT NULL,
    `port` INT(10) NOT NULL,
    `received` VARCHAR(128) DEFAULT NULL,
    `received_port` INT(10) UNSIGNED DEFAULT NULL,
    `received_proto` INT(10) UNSIGNED DEFAULT NULL,
    `path` VARCHAR(512) DEFAULT NULL,
    `rinstance` VARCHAR(255) DEFAULT NULL,
    `rx_session_id` VARCHAR(256) DEFAULT NULL,
    `reg_state` TINYINT(4) DEFAULT NULL,
    `expires` DATETIME DEFAULT '2030-05-28 21:32:15',
    `service_routes` VARCHAR(2048) DEFAULT NULL,
    `socket` VARCHAR(64) DEFAULT NULL,
    `public_ids` VARCHAR(2048) DEFAULT NULL,
    `security_type` INT(11) DEFAULT NULL,
    `protocol` VARCHAR(5) DEFAULT NULL,
    `mode` VARCHAR(10) DEFAULT NULL,
    `ck` VARCHAR(100) DEFAULT NULL,
    `ik` VARCHAR(100) DEFAULT NULL,
    `ealg` VARCHAR(20) DEFAULT NULL,
    `ialg` VARCHAR(20) DEFAULT NULL,
    `port_pc` INT(11) UNSIGNED DEFAULT NULL,
    `port_ps` INT(11) UNSIGNED DEFAULT NULL,
    `port_uc` INT(11) UNSIGNED DEFAULT NULL,
    `port_us` INT(11) UNSIGNED DEFAULT NULL,
    `spi_pc` INT(11) UNSIGNED DEFAULT NULL,
    `spi_ps` INT(11) UNSIGNED DEFAULT NULL,
    `spi_uc` INT(11) UNSIGNED DEFAULT NULL,
    `spi_us` INT(11) UNSIGNED DEFAULT NULL,
    `t_security_type` INT(11) DEFAULT NULL,
    `t_protocol` VARCHAR(5) DEFAULT NULL,
    `t_mode` VARCHAR(10) DEFAULT NULL,
    `t_ck` VARCHAR(100) DEFAULT NULL,
    `t_ik` VARCHAR(100) DEFAULT NULL,
    `t_ealg` VARCHAR(20) DEFAULT NULL,
    `t_ialg` VARCHAR(20) DEFAULT NULL,
    `t_port_pc` INT(11) UNSIGNED DEFAULT NULL,
    `t_port_ps` INT(11) UNSIGNED DEFAULT NULL,
    `t_port_uc` INT(11) UNSIGNED DEFAULT NULL,
    `t_port_us` INT(11) UNSIGNED DEFAULT NULL,
    `t_spi_pc` INT(11) UNSIGNED DEFAULT NULL,
    `t_spi_ps` INT(11) UNSIGNED DEFAULT NULL,
    `t_spi_uc` INT(11) UNSIGNED DEFAULT NULL,
    `t_spi_us` INT(11) UNSIGNED DEFAULT NULL,
    `instance_id` VARCHAR(255) DEFAULT NULL,
    `pub_gruu` VARCHAR(255) DEFAULT NULL,
    `temp_gruu` VARCHAR(255) DEFAULT NULL,
    `public_ids_barred` VARCHAR(2048) DEFAULT NULL
);

CREATE INDEX aor_idx ON pcscf_location (`aor`);

INSERT INTO version (table_name, table_version) values ('pcscf_location','8');

CREATE TABLE `pcscf_gruu_history` (
    `id` INT(10) UNSIGNED AUTO_INCREMENT PRIMARY KEY NOT NULL,
    `location_id` INT(10) UNSIGNED NOT NULL,
    `temp_gruu` VARCHAR(255) NOT NULL,
    `created` DATETIME NOT NULL,
    `expires` DATETIME NOT NULL
);

CREATE INDEX idx_loc ON pcscf_gruu_history (`location_id`);
CREATE INDEX idx_tgruu ON pcscf_gruu_history (`temp_gruu`);
CREATE INDEX idx_exp ON pcscf_gruu_history (`expires`);

INSERT INTO version (table_name, table_version) values ('pcscf_gruu_history','1');

