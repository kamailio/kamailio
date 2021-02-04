CREATE TABLE secfilter (
    id NUMBER(10) PRIMARY KEY,
    action NUMBER(5) DEFAULT 0 NOT NULL,
    type NUMBER(5) DEFAULT 0 NOT NULL,
    data VARCHAR2(64) DEFAULT ''
);

CREATE OR REPLACE TRIGGER secfilter_tr
before insert on secfilter FOR EACH ROW
BEGIN
  auto_id(:NEW.id);
END secfilter_tr;
/
BEGIN map2users('secfilter'); END;
/
CREATE INDEX secfilter_secfilter_idx  ON secfilter (action, type, data);

INSERT INTO version (table_name, table_version) values ('secfilter','1');

