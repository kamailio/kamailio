INSERT INTO version (table_name, table_version) values ('purplemap','1');
CREATE TABLE purplemap (
    id NUMBER(10) PRIMARY KEY,
    sip_user VARCHAR2(128),
    ext_user VARCHAR2(128),
    ext_prot VARCHAR2(16),
    ext_pass VARCHAR2(64)
);

CREATE OR REPLACE TRIGGER purplemap_tr
before insert on purplemap FOR EACH ROW
BEGIN
  auto_id(:NEW.id);
END purplemap_tr;
/
BEGIN map2users('purplemap'); END;
/
