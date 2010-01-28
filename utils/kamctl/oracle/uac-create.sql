INSERT INTO version (table_name, table_version) values ('uacreg','1');
CREATE TABLE uacreg (
    id NUMBER(10) PRIMARY KEY,
    l_uuid VARCHAR2(64) DEFAULT '',
    l_username VARCHAR2(64) DEFAULT '',
    l_domain VARCHAR2(128) DEFAULT '',
    r_username VARCHAR2(64) DEFAULT '',
    r_domain VARCHAR2(128) DEFAULT '',
    realm VARCHAR2(64) DEFAULT '',
    auth_username VARCHAR2(64) DEFAULT '',
    auth_password VARCHAR2(64) DEFAULT '',
    auth_proxy VARCHAR2(64) DEFAULT '',
    expires NUMBER(10) DEFAULT 0 NOT NULL,
    CONSTRAINT uacreg_l_uuid_idx  UNIQUE (l_uuid)
);

CREATE OR REPLACE TRIGGER uacreg_tr
before insert on uacreg FOR EACH ROW
BEGIN
  auto_id(:NEW.id);
END uacreg_tr;
/
BEGIN map2users('uacreg'); END;
/
