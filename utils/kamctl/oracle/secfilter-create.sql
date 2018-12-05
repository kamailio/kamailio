CREATE TABLE security (
  id NUMBER(10) PRIMARY KEY,
  action NUMBER(1) DEFAULT 0 NOT NULL,
  type NUMBER(1) DEFAULT 0 NOT NULL,
  data VARCHAR2(64) DEFAULT '' NOT NULL
);

CREATE OR REPLACE TRIGGER security_tr
before insert on security FOR EACH ROW
BEGIN
  auto_id(:NEW.id);
END security_tr;
/
BEGIN map2users('security'); END;
/
CREATE INDEX ORA_security_idx  ON security (action, type, data);

INSERT INTO version (table_name, table_version) values ('security', '1');

