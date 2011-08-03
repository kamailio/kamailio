INSERT INTO version (table_name, table_version) values ('pl_pipes','1');
CREATE TABLE pl_pipes (
    id NUMBER(10) PRIMARY KEY,
    pipeid VARCHAR2(64) DEFAULT '',
    algorithm VARCHAR2(32) DEFAULT '',
    plimit NUMBER(10) DEFAULT 0 NOT NULL
);

CREATE OR REPLACE TRIGGER pl_pipes_tr
before insert on pl_pipes FOR EACH ROW
BEGIN
  auto_id(:NEW.id);
END pl_pipes_tr;
/
BEGIN map2users('pl_pipes'); END;
/
