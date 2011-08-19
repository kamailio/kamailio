INSERT INTO version (table_name, table_version) values ('htable','2');
CREATE TABLE htable (
    id NUMBER(10) PRIMARY KEY,
    key_name VARCHAR2(64) DEFAULT '',
    key_type NUMBER(10) DEFAULT 0 NOT NULL,
    value_type NUMBER(10) DEFAULT 0 NOT NULL,
    key_value VARCHAR2(128) DEFAULT '',
    expires NUMBER(10) DEFAULT 0 NOT NULL
);

CREATE OR REPLACE TRIGGER htable_tr
before insert on htable FOR EACH ROW
BEGIN
  auto_id(:NEW.id);
END htable_tr;
/
BEGIN map2users('htable'); END;
/
