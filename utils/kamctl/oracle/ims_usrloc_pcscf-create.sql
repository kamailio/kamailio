CREATE TABLE pcscf_location (
    id NUMBER(10) PRIMARY KEY,
    domain VARCHAR2(64),
    aor VARCHAR2(255),
    host VARCHAR2(100),
    port NUMBER(10),
    received VARCHAR2(128) DEFAULT NULL,
    received_port NUMBER(10) DEFAULT NULL,
    received_proto NUMBER(10) DEFAULT NULL,
    path VARCHAR2(512) DEFAULT NULL,
    rinstance VARCHAR2(255) DEFAULT NULL,
    rx_session_id VARCHAR2(256) DEFAULT NULL,
    reg_state NUMBER(5) DEFAULT NULL,
    expires DATE DEFAULT to_date('2030-05-28 21:32:15','yyyy-mm-dd hh24:mi:ss'),
    service_routes VARCHAR2(2048) DEFAULT NULL,
    socket VARCHAR2(64) DEFAULT NULL,
    public_ids VARCHAR2(2048) DEFAULT NULL,
    security_type NUMBER(10) DEFAULT NULL,
    protocol VARCHAR2(5) DEFAULT NULL,
    mode VARCHAR2(10) DEFAULT NULL,
    ck VARCHAR2(100) DEFAULT NULL,
    ik VARCHAR2(100) DEFAULT NULL,
    ealg VARCHAR2(20) DEFAULT NULL,
    ialg VARCHAR2(20) DEFAULT NULL,
    port_pc NUMBER(10) DEFAULT NULL,
    port_ps NUMBER(10) DEFAULT NULL,
    port_uc NUMBER(10) DEFAULT NULL,
    port_us NUMBER(10) DEFAULT NULL,
    spi_pc NUMBER(10) DEFAULT NULL,
    spi_ps NUMBER(10) DEFAULT NULL,
    spi_uc NUMBER(10) DEFAULT NULL,
    spi_us NUMBER(10) DEFAULT NULL,
    t_security_type NUMBER(10) DEFAULT NULL,
    t_protocol VARCHAR2(5) DEFAULT NULL,
    t_mode VARCHAR2(10) DEFAULT NULL,
    t_ck VARCHAR2(100) DEFAULT NULL,
    t_ik VARCHAR2(100) DEFAULT NULL,
    t_ealg VARCHAR2(20) DEFAULT NULL,
    t_ialg VARCHAR2(20) DEFAULT NULL,
    t_port_pc NUMBER(10) DEFAULT NULL,
    t_port_ps NUMBER(10) DEFAULT NULL,
    t_port_uc NUMBER(10) DEFAULT NULL,
    t_port_us NUMBER(10) DEFAULT NULL,
    t_spi_pc NUMBER(10) DEFAULT NULL,
    t_spi_ps NUMBER(10) DEFAULT NULL,
    t_spi_uc NUMBER(10) DEFAULT NULL,
    t_spi_us NUMBER(10) DEFAULT NULL
);

CREATE OR REPLACE TRIGGER pcscf_location_tr
before insert on pcscf_location FOR EACH ROW
BEGIN
  auto_id(:NEW.id);
END pcscf_location_tr;
/
BEGIN map2users('pcscf_location'); END;
/
CREATE INDEX pcscf_location_aor_idx  ON pcscf_location (aor);

INSERT INTO version (table_name, table_version) values ('pcscf_location','7');

