INSERT INTO version (table_name, table_version) values ('uid_user_attrs','3');
CREATE TABLE uid_user_attrs (
    id NUMBER(10) PRIMARY KEY,
    uuid VARCHAR2(64),
    name VARCHAR2(32),
    value VARCHAR2(128),
    type NUMBER(10) DEFAULT 0 NOT NULL,
    flags NUMBER(10) DEFAULT 0 NOT NULL,
    CONSTRAINT uid_user_attrs_userattrs_idx  UNIQUE (uuid, name, value)
);

CREATE OR REPLACE TRIGGER uid_user_attrs_tr
before insert on uid_user_attrs FOR EACH ROW
BEGIN
  auto_id(:NEW.id);
END uid_user_attrs_tr;
/
BEGIN map2users('uid_user_attrs'); END;
/
