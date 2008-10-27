INSERT INTO version (table_name, table_version) values ('gw','8');
CREATE TABLE gw (
    id NUMBER(10) PRIMARY KEY,
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
    CONSTRAINT gw_gw_name_idx  UNIQUE (gw_name)
);

CREATE OR REPLACE TRIGGER gw_tr
before insert on gw FOR EACH ROW
BEGIN
  auto_id(:NEW.id);
END gw_tr;
/
BEGIN map2users('gw'); END;
/
CREATE INDEX gw_grp_id_idx  ON gw (grp_id);

INSERT INTO version (table_name, table_version) values ('lcr','2');
CREATE TABLE lcr (
    id NUMBER(10) PRIMARY KEY,
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
CREATE INDEX lcr_prefix_idx  ON lcr (prefix);
CREATE INDEX lcr_from_uri_idx  ON lcr (from_uri);
CREATE INDEX lcr_grp_id_idx  ON lcr (grp_id);

