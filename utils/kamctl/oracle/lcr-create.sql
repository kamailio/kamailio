INSERT INTO version (table_name, table_version) values ('lcr_gw','3');
CREATE TABLE lcr_gw (
    id NUMBER(10) PRIMARY KEY,
    lcr_id NUMBER(5),
    gw_name VARCHAR2(128),
    ip_addr VARCHAR2(50),
    hostname VARCHAR2(64),
    port NUMBER(5),
    params VARCHAR2(64),
    uri_scheme NUMBER(5),
    transport NUMBER(5),
    strip NUMBER(5),
    prefix VARCHAR2(16) DEFAULT NULL,
    tag VARCHAR2(64) DEFAULT NULL,
    flags NUMBER(10) DEFAULT 0 NOT NULL,
    defunct NUMBER(10) DEFAULT NULL
);

CREATE OR REPLACE TRIGGER lcr_gw_tr
before insert on lcr_gw FOR EACH ROW
BEGIN
  auto_id(:NEW.id);
END lcr_gw_tr;
/
BEGIN map2users('lcr_gw'); END;
/
CREATE INDEX lcr_gw_lcr_id_idx  ON lcr_gw (lcr_id);

INSERT INTO version (table_name, table_version) values ('lcr_rule_target','1');
CREATE TABLE lcr_rule_target (
    id NUMBER(10) PRIMARY KEY,
    lcr_id NUMBER(5),
    rule_id NUMBER(10),
    gw_id NUMBER(10),
    priority NUMBER(5),
    weight NUMBER(10) DEFAULT 1 NOT NULL,
    CONSTRAINT ORA_rule_id_gw_id_idx  UNIQUE (rule_id, gw_id)
);

CREATE OR REPLACE TRIGGER lcr_rule_target_tr
before insert on lcr_rule_target FOR EACH ROW
BEGIN
  auto_id(:NEW.id);
END lcr_rule_target_tr;
/
BEGIN map2users('lcr_rule_target'); END;
/
CREATE INDEX lcr_rule_target_lcr_id_idx  ON lcr_rule_target (lcr_id);

INSERT INTO version (table_name, table_version) values ('lcr_rule','2');
CREATE TABLE lcr_rule (
    id NUMBER(10) PRIMARY KEY,
    lcr_id NUMBER(5),
    prefix VARCHAR2(16) DEFAULT NULL,
    from_uri VARCHAR2(64) DEFAULT NULL,
    request_uri VARCHAR2(64) DEFAULT NULL,
    stopper NUMBER(10) DEFAULT 0 NOT NULL,
    enabled NUMBER(10) DEFAULT 1 NOT NULL,
    CONSTRAINT ORA_lcr_id_prefix_from_uri_idx  UNIQUE (lcr_id, prefix, from_uri)
);

CREATE OR REPLACE TRIGGER lcr_rule_tr
before insert on lcr_rule FOR EACH ROW
BEGIN
  auto_id(:NEW.id);
END lcr_rule_tr;
/
BEGIN map2users('lcr_rule'); END;
/
