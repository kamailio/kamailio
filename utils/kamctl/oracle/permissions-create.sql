INSERT INTO version (table_name, table_version) values ('trusted','5');
CREATE TABLE trusted (
    id NUMBER(10) PRIMARY KEY,
    src_ip VARCHAR2(50),
    proto VARCHAR2(4),
    from_pattern VARCHAR2(64) DEFAULT NULL,
    tag VARCHAR2(64)
);

CREATE OR REPLACE TRIGGER trusted_tr
before insert on trusted FOR EACH ROW
BEGIN
  auto_id(:NEW.id);
END trusted_tr;
/
BEGIN map2users('trusted'); END;
/
CREATE INDEX trusted_peer_idx  ON trusted (src_ip);

INSERT INTO version (table_name, table_version) values ('address','6');
CREATE TABLE address (
    id NUMBER(10) PRIMARY KEY,
    grp NUMBER(10) DEFAULT 1 NOT NULL,
    ip_addr VARCHAR2(50),
    mask NUMBER(10) DEFAULT 32 NOT NULL,
    port NUMBER(5) DEFAULT 0 NOT NULL,
    tag VARCHAR2(64)
);

CREATE OR REPLACE TRIGGER address_tr
before insert on address FOR EACH ROW
BEGIN
  auto_id(:NEW.id);
END address_tr;
/
BEGIN map2users('address'); END;
/
