INSERT INTO version (table_name, table_version) values ('pdt','1');
CREATE TABLE pdt (
    id NUMBER(10) PRIMARY KEY,
    sdomain VARCHAR2(128),
    prefix VARCHAR2(32),
    domain VARCHAR2(128) DEFAULT '',
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
