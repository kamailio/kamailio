INSERT INTO version (table_name, table_version) values ('domain','1');
CREATE TABLE domain (
    id NUMBER(10) PRIMARY KEY,
    domain VARCHAR2(64) DEFAULT '',
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
