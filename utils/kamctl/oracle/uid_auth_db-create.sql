INSERT INTO version (table_name, table_version) values ('uid_credentials','7');
CREATE TABLE uid_credentials (
    id NUMBER(10) PRIMARY KEY,
    auth_username VARCHAR2(64),
    did VARCHAR2(64) DEFAULT '_default',
    realm VARCHAR2(64),
    password VARCHAR2(28) DEFAULT '',
    flags NUMBER(10) DEFAULT 0 NOT NULL,
    ha1 VARCHAR2(32),
    ha1b VARCHAR2(32) DEFAULT '',
    uuid VARCHAR2(64)
);

CREATE OR REPLACE TRIGGER uid_credentials_tr
before insert on uid_credentials FOR EACH ROW
BEGIN
  auto_id(:NEW.id);
END uid_credentials_tr;
/
BEGIN map2users('uid_credentials'); END;
/
CREATE INDEX uid_credentials_cred_idx  ON uid_credentials (auth_username, did);
CREATE INDEX uid_credentials_uuid  ON uid_credentials (uuid);
CREATE INDEX uid_credentials_did_idx  ON uid_credentials (did);
CREATE INDEX uid_credentials_realm_idx  ON uid_credentials (realm);

