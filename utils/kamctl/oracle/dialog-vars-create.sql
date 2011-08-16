INSERT INTO version (table_name, table_version) values ('dialog_vars','1');
CREATE TABLE dialog_vars (
    id NUMBER(10) PRIMARY KEY,
    hash_entry NUMBER(10),
    hash_id NUMBER(10),
    dialog_key VARCHAR2(128),
    dialog_value VARCHAR2(512)
);

CREATE OR REPLACE TRIGGER dialog_vars_tr
before insert on dialog_vars FOR EACH ROW
BEGIN
  auto_id(:NEW.id);
END dialog_vars_tr;
/
BEGIN map2users('dialog_vars'); END;
/
CREATE INDEX dialog_vars_hash_idx  ON dialog_vars (hash_entry, hash_id);

