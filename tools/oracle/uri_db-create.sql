INSERT INTO version (table_name, table_version) values ('uri','1');
CREATE TABLE uri (
    id NUMBER(10) PRIMARY KEY,
    username VARCHAR2(64) DEFAULT '',
    domain VARCHAR2(64) DEFAULT '',
    uri_user VARCHAR2(64) DEFAULT '',
    last_modified DATE DEFAULT to_date('1900-01-01 00:00:01','yyyy-mm-dd hh24:mi:ss'),
    CONSTRAINT uri_account_idx  UNIQUE (username, domain, uri_user)
);

CREATE OR REPLACE TRIGGER uri_tr
before insert on uri FOR EACH ROW
BEGIN
  auto_id(:NEW.id);
END uri_tr;
/
BEGIN map2users('uri'); END;
/
