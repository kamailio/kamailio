CREATE TABLE nds_trusted_domains (
    id NUMBER(10) PRIMARY KEY,
    trusted_domain VARCHAR2(83) DEFAULT ''
);

CREATE OR REPLACE TRIGGER nds_trusted_domains_tr
before insert on nds_trusted_domains FOR EACH ROW
BEGIN
  auto_id(:NEW.id);
END nds_trusted_domains_tr;
/
BEGIN map2users('nds_trusted_domains'); END;
/
INSERT INTO version (table_name, table_version) values ('nds_trusted_domains','1');

CREATE TABLE s_cscf (
    id NUMBER(10) PRIMARY KEY,
    name VARCHAR2(83) DEFAULT '',
    s_cscf_uri VARCHAR2(83) DEFAULT ''
);

CREATE OR REPLACE TRIGGER s_cscf_tr
before insert on s_cscf FOR EACH ROW
BEGIN
  auto_id(:NEW.id);
END s_cscf_tr;
/
BEGIN map2users('s_cscf'); END;
/
INSERT INTO version (table_name, table_version) values ('s_cscf','1');

CREATE TABLE s_cscf_capabilities (
    id NUMBER(10) PRIMARY KEY,
    id_s_cscf NUMBER(10) DEFAULT 0 NOT NULL,
    capability NUMBER(10) DEFAULT 0 NOT NULL
);

CREATE OR REPLACE TRIGGER s_cscf_capabilities_tr
before insert on s_cscf_capabilities FOR EACH ROW
BEGIN
  auto_id(:NEW.id);
END s_cscf_capabilities_tr;
/
BEGIN map2users('s_cscf_capabilities'); END;
/
CREATE INDEX ORA_idx_capability  ON s_cscf_capabilities (capability);
CREATE INDEX ORA_idx_id_s_cscf  ON s_cscf_capabilities (id_s_cscf);

INSERT INTO version (table_name, table_version) values ('s_cscf_capabilities','1');
