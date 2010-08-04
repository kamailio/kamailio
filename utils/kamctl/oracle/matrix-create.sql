INSERT INTO version (table_name, table_version) values ('matrix','1');
CREATE TABLE matrix (
    first NUMBER(10),
    second NUMBER(5),
    res NUMBER(10)
);

CREATE OR REPLACE TRIGGER matrix_tr
before insert on matrix FOR EACH ROW
BEGIN
  auto_id(:NEW.id);
END matrix_tr;
/
BEGIN map2users('matrix'); END;
/
CREATE INDEX matrix_matrix_idx  ON matrix (first, second);

