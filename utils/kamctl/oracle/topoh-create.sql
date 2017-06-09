CREATE TABLE topoh_address (
    id NUMBER(10) PRIMARY KEY,
    trust NUMBER(10) DEFAULT 1 NOT NULL,
    ip_addr VARCHAR2(50),
    mask NUMBER(10) DEFAULT 32 NOT NULL,
    port NUMBER(5) DEFAULT 0 NOT NULL,
    tag VARCHAR2(64)
);

CREATE OR REPLACE TRIGGER topoh_address_tr
before insert on topoh_address FOR EACH ROW
BEGIN
  auto_id(:NEW.id);
END topoh_address_tr;
/
BEGIN map2users('topoh_address'); END;
/
INSERT INTO version (table_name, table_version) values ('topoh_address','1');

