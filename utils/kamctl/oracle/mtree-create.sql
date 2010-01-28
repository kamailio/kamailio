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
