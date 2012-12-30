INSERT INTO version (table_name, table_version) values ('uid_uri','3');
CREATE TABLE uid_uri (
    id NUMBER(10) PRIMARY KEY,
    uuid VARCHAR2(64),
    did VARCHAR2(64),
    username VARCHAR2(64),
    flags NUMBER(10) DEFAULT 0 NOT NULL,
    scheme VARCHAR2(8) DEFAULT 'sip'
);

CREATE OR REPLACE TRIGGER uid_uri_tr
before insert on uid_uri FOR EACH ROW
BEGIN
  auto_id(:NEW.id);
END uid_uri_tr;
/
BEGIN map2users('uid_uri'); END;
/
CREATE INDEX uid_uri_uri_idx1  ON uid_uri (username, did, scheme);
CREATE INDEX uid_uri_uri_uid  ON uid_uri (uuid);

INSERT INTO version (table_name, table_version) values ('uid_uri_attrs','2');
CREATE TABLE uid_uri_attrs (
    id NUMBER(10) PRIMARY KEY,
    username VARCHAR2(64),
    did VARCHAR2(64),
    name VARCHAR2(32),
    value VARCHAR2(128),
    type NUMBER(10) DEFAULT 0 NOT NULL,
    flags NUMBER(10) DEFAULT 0 NOT NULL,
    scheme VARCHAR2(8) DEFAULT 'sip',
    CONSTRAINT uid_uri_attrs_uriattrs_idx  UNIQUE (username, did, name, value, scheme)
);

CREATE OR REPLACE TRIGGER uid_uri_attrs_tr
before insert on uid_uri_attrs FOR EACH ROW
BEGIN
  auto_id(:NEW.id);
END uid_uri_attrs_tr;
/
BEGIN map2users('uid_uri_attrs'); END;
/
