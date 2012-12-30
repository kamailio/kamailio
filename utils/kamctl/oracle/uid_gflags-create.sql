INSERT INTO version (table_name, table_version) values ('uid_global_attrs','1');
CREATE TABLE uid_global_attrs (
    id NUMBER(10) PRIMARY KEY,
    name VARCHAR2(32),
    type NUMBER(10) DEFAULT 0 NOT NULL,
    value VARCHAR2(128),
    flags NUMBER(10) DEFAULT 0 NOT NULL,
    CONSTRAINT ORA_global_attrs_idx  UNIQUE (name, value)
);

CREATE OR REPLACE TRIGGER uid_global_attrs_tr
before insert on uid_global_attrs FOR EACH ROW
BEGIN
  auto_id(:NEW.id);
END uid_global_attrs_tr;
/
BEGIN map2users('uid_global_attrs'); END;
/
