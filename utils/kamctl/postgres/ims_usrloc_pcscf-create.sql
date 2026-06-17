CREATE TABLE pcscf_location (
    id SERIAL PRIMARY KEY NOT NULL,
    domain VARCHAR(64) NOT NULL,
    aor VARCHAR(255) NOT NULL,
    host VARCHAR(100) NOT NULL,
    port INTEGER NOT NULL,
    received VARCHAR(128) DEFAULT NULL,
    received_port INTEGER DEFAULT NULL,
    received_proto INTEGER DEFAULT NULL,
    path VARCHAR(512) DEFAULT NULL,
    rinstance VARCHAR(255) DEFAULT NULL,
    rx_session_id VARCHAR(256) DEFAULT NULL,
    reg_state SMALLINT DEFAULT NULL,
    expires TIMESTAMP WITHOUT TIME ZONE DEFAULT '2030-05-28 21:32:15',
    service_routes VARCHAR(2048) DEFAULT NULL,
    socket VARCHAR(64) DEFAULT NULL,
    public_ids VARCHAR(2048) DEFAULT NULL,
    security_type INTEGER DEFAULT NULL,
    protocol VARCHAR(5) DEFAULT NULL,
    mode VARCHAR(10) DEFAULT NULL,
    ck VARCHAR(100) DEFAULT NULL,
    ik VARCHAR(100) DEFAULT NULL,
    ealg VARCHAR(20) DEFAULT NULL,
    ialg VARCHAR(20) DEFAULT NULL,
    port_pc INTEGER DEFAULT NULL,
    port_ps INTEGER DEFAULT NULL,
    port_uc INTEGER DEFAULT NULL,
    port_us INTEGER DEFAULT NULL,
    spi_pc INTEGER DEFAULT NULL,
    spi_ps INTEGER DEFAULT NULL,
    spi_uc INTEGER DEFAULT NULL,
    spi_us INTEGER DEFAULT NULL,
    t_security_type INTEGER DEFAULT NULL,
    t_protocol VARCHAR(5) DEFAULT NULL,
    t_mode VARCHAR(10) DEFAULT NULL,
    t_ck VARCHAR(100) DEFAULT NULL,
    t_ik VARCHAR(100) DEFAULT NULL,
    t_ealg VARCHAR(20) DEFAULT NULL,
    t_ialg VARCHAR(20) DEFAULT NULL,
    t_port_pc INTEGER DEFAULT NULL,
    t_port_ps INTEGER DEFAULT NULL,
    t_port_uc INTEGER DEFAULT NULL,
    t_port_us INTEGER DEFAULT NULL,
    t_spi_pc INTEGER DEFAULT NULL,
    t_spi_ps INTEGER DEFAULT NULL,
    t_spi_uc INTEGER DEFAULT NULL,
    t_spi_us INTEGER DEFAULT NULL,
    instance_id VARCHAR(255) DEFAULT NULL,
    pub_gruu VARCHAR(255) DEFAULT NULL,
    temp_gruu VARCHAR(255) DEFAULT NULL,
    public_ids_barred VARCHAR(2048) DEFAULT NULL
);

CREATE INDEX pcscf_location_aor_idx ON pcscf_location (aor);

INSERT INTO version (table_name, table_version) values ('pcscf_location','8');

CREATE TABLE pcscf_temp_gruu_history (
    id SERIAL PRIMARY KEY NOT NULL,
    location_id INTEGER NOT NULL,
    temp_gruu VARCHAR(255) NOT NULL,
    created TIMESTAMP WITHOUT TIME ZONE NOT NULL,
    expires TIMESTAMP WITHOUT TIME ZONE NOT NULL
);

CREATE INDEX pcscf_temp_gruu_history_idx_loc ON pcscf_temp_gruu_history (location_id);
CREATE INDEX pcscf_temp_gruu_history_idx_tgruu ON pcscf_temp_gruu_history (temp_gruu);
CREATE INDEX pcscf_temp_gruu_history_idx_exp ON pcscf_temp_gruu_history (expires);

INSERT INTO version (table_name, table_version) values ('pcscf_temp_gruu_history','1');

