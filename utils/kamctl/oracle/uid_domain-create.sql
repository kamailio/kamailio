INSERT INTO version (table_name, table_version) values ('uid_domain','2');
CREATE TABLE uid_domain (
    id NUMBER(10) PRIMARY KEY,
    did VARCHAR2(64),
    domain VARCHAR2(64),
    flags NUMBER(10) DEFAULT 0 NOT NULL,
    CONSTRAINT uid_domain_domain_idx  UNIQUE (domain)
);

CREATE OR REPLACE TRIGGER uid_domain_tr
before insert on uid_domain FOR EACH ROW
BEGIN
  auto_id(:NEW.id);
END uid_domain_tr;
/
BEGIN map2users('uid_domain'); END;
/
CREATE INDEX uid_domain_did_idx  ON uid_domain (did);

INSERT INTO version (table_name, table_version) values ('uid_domain_attrs','1');
CREATE TABLE uid_domain_attrs (
    id NUMBER(10) PRIMARY KEY,
    did VARCHAR2(64),
    name VARCHAR2(32),
    type NUMBER(10) DEFAULT 0 NOT NULL,
    value VARCHAR2(128),
    flags NUMBER(10) DEFAULT 0 NOT NULL,
    CONSTRAINT ORA_domain_attr_idx  UNIQUE (did, name, value)
);

CREATE OR REPLACE TRIGGER uid_domain_attrs_tr
before insert on uid_domain_attrs FOR EACH ROW
BEGIN
  auto_id(:NEW.id);
END uid_domain_attrs_tr;
/
BEGIN map2users('uid_domain_attrs'); END;
/
CREATE INDEX uid_domain_attrs_domain_did  ON uid_domain_attrs (did, flags);

