INSERT INTO version (table_name, table_version) values ('mtree','1');
CREATE TABLE mtree (
    id NUMBER(10) PRIMARY KEY,
    tprefix VARCHAR2(32) DEFAULT '',
    tvalue VARCHAR2(128) DEFAULT '',
    CONSTRAINT mtree_tprefix_idx  UNIQUE (tprefix)
);

CREATE OR REPLACE TRIGGER mtree_tr
before insert on mtree FOR EACH ROW
BEGIN
  auto_id(:NEW.id);
END mtree_tr;
/
BEGIN map2users('mtree'); END;
/
INSERT INTO version (table_name, table_version) values ('mtrees','2');
CREATE TABLE mtrees (
    id NUMBER(10) PRIMARY KEY,
    tname VARCHAR2(128) DEFAULT '',
    tprefix VARCHAR2(32) DEFAULT '',
    tvalue VARCHAR2(128) DEFAULT '',
    CONSTRAINT ORA_tname_tprefix_tvalue_idx  UNIQUE (tname, tprefix, tvalue)
);

CREATE OR REPLACE TRIGGER mtrees_tr
before insert on mtrees FOR EACH ROW
BEGIN
  auto_id(:NEW.id);
END mtrees_tr;
/
BEGIN map2users('mtrees'); END;
/
