INSERT INTO version (table_name, table_version) values ('gw','10');
CREATE TABLE gw (
    id NUMBER(10) PRIMARY KEY,
    lcr_id NUMBER(5),
    gw_name VARCHAR2(128),
    grp_id NUMBER(10),
    ip_addr VARCHAR2(15),
    hostname VARCHAR2(64),
    port NUMBER(5),
    uri_scheme NUMBER(5),
    transport NUMBER(5),
    strip NUMBER(5),
    tag VARCHAR2(16) DEFAULT NULL,
    weight NUMBER(10),
    flags NUMBER(10) DEFAULT 0 NOT NULL,
    defunct NUMBER(10) DEFAULT NULL,
    CONSTRAINT gw_lcr_id_grp_id_gw_name_idx  UNIQUE (lcr_id, grp_id, gw_name),
    CONSTRAINT gw_lcr_id_grp_id_ip_addr_idx  UNIQUE (lcr_id, grp_id, ip_addr)
);

CREATE OR REPLACE TRIGGER gw_tr
before insert on gw FOR EACH ROW
BEGIN
  auto_id(:NEW.id);
END gw_tr;
/
BEGIN map2users('gw'); END;
/
INSERT INTO version (table_name, table_version) values ('lcr','3');
CREATE TABLE lcr (
    id NUMBER(10) PRIMARY KEY,
    lcr_id NUMBER(5),
    prefix VARCHAR2(16) DEFAULT NULL,
    from_uri VARCHAR2(64) DEFAULT NULL,
    grp_id NUMBER(10),
    priority NUMBER(5)
);

CREATE OR REPLACE TRIGGER lcr_tr
before insert on lcr FOR EACH ROW
BEGIN
  auto_id(:NEW.id);
END lcr_tr;
/
BEGIN map2users('lcr'); END;
/
CREATE INDEX lcr_lcr_id_idx  ON lcr (lcr_id);

