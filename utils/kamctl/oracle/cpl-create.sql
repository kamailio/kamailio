INSERT INTO version (table_name, table_version) values ('cpl','1');
CREATE TABLE cpl (
    id NUMBER(10) PRIMARY KEY,
    username VARCHAR2(64),
    domain VARCHAR2(64) DEFAULT '',
    cpl_xml CLOB,
    cpl_bin CLOB,
    CONSTRAINT cpl_account_idx  UNIQUE (username, domain)
);

CREATE OR REPLACE TRIGGER cpl_tr
before insert on cpl FOR EACH ROW
BEGIN
  auto_id(:NEW.id);
END cpl_tr;
/
BEGIN map2users('cpl'); END;
/
