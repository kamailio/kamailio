CREATE TABLE pdt (
    id NUMBER(10) PRIMARY KEY,
    sdomain VARCHAR2(255),
    prefix VARCHAR2(32),
    domain VARCHAR2(255) DEFAULT '',
    CONSTRAINT pdt_sdomain_prefix_idx  UNIQUE (sdomain, prefix)
);

CREATE OR REPLACE TRIGGER pdt_tr
before insert on pdt FOR EACH ROW
BEGIN
  auto_id(:NEW.id);
END pdt_tr;
/
BEGIN map2users('pdt'); END;
/
INSERT INTO version (table_name, table_version) values ('pdt','1');

