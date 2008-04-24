INSERT INTO version (table_name, table_version) values ('subscriber','6');
CREATE TABLE subscriber (
    id NUMBER(10) PRIMARY KEY,
    username VARCHAR2(64) DEFAULT '',
    domain VARCHAR2(64) DEFAULT '',
    password VARCHAR2(25) DEFAULT '',
    email_address VARCHAR2(64) DEFAULT '',
    ha1 VARCHAR2(64) DEFAULT '',
    ha1b VARCHAR2(64) DEFAULT '',
    rpid VARCHAR2(64) DEFAULT NULL,
    CONSTRAINT subscriber_account_idx  UNIQUE (username, domain)
);

CREATE OR REPLACE TRIGGER subscriber_tr
before insert on subscriber FOR EACH ROW
BEGIN
  auto_id(:NEW.id);
END subscriber_tr;
/
BEGIN map2users('subscriber'); END;
/
CREATE INDEX subscriber_username_idx  ON subscriber (username);

