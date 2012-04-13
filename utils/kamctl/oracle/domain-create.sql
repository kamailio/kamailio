INSERT INTO version (table_name, table_version) values ('domain','2');
CREATE TABLE domain (
    id NUMBER(10) PRIMARY KEY,
    domain VARCHAR2(64),
    did VARCHAR2(64) DEFAULT NULL,
    last_modified DATE DEFAULT to_date('1900-01-01 00:00:01','yyyy-mm-dd hh24:mi:ss'),
    CONSTRAINT domain_domain_idx  UNIQUE (domain)
);

CREATE OR REPLACE TRIGGER domain_tr
before insert on domain FOR EACH ROW
BEGIN
  auto_id(:NEW.id);
END domain_tr;
/
BEGIN map2users('domain'); END;
/
INSERT INTO version (table_name, table_version) values ('domain_attrs','1');
CREATE TABLE domain_attrs (
    id NUMBER(10) PRIMARY KEY,
    did VARCHAR2(64),
    name VARCHAR2(32),
    type NUMBER(10),
    value VARCHAR2(255),
    last_modified DATE DEFAULT to_date('1900-01-01 00:00:01','yyyy-mm-dd hh24:mi:ss'),
    CONSTRAINT domain_attrs_domain_attrs_idx  UNIQUE (did, name, value)
);

CREATE OR REPLACE TRIGGER domain_attrs_tr
before insert on domain_attrs FOR EACH ROW
BEGIN
  auto_id(:NEW.id);
END domain_attrs_tr;
/
BEGIN map2users('domain_attrs'); END;
/
