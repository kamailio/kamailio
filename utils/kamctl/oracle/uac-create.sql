CREATE TABLE uacreg (
    id NUMBER(10) PRIMARY KEY,
    l_uuid VARCHAR2(64) DEFAULT '',
    l_username VARCHAR2(64) DEFAULT '',
    l_domain VARCHAR2(64) DEFAULT '',
    r_username VARCHAR2(64) DEFAULT '',
    r_domain VARCHAR2(64) DEFAULT '',
    realm VARCHAR2(64) DEFAULT '',
    auth_username VARCHAR2(64) DEFAULT '',
    auth_password VARCHAR2(64) DEFAULT '',
    auth_ha1 VARCHAR2(128) DEFAULT '',
    auth_proxy VARCHAR2(255) DEFAULT '',
    expires NUMBER(10) DEFAULT 0 NOT NULL,
    flags NUMBER(10) DEFAULT 0 NOT NULL,
    reg_delay NUMBER(10) DEFAULT 0 NOT NULL,
    socket VARCHAR2(128) DEFAULT '',
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
INSERT INTO version (table_name, table_version) values ('uacreg','4');

