CREATE TABLE contact (
    id NUMBER(10) PRIMARY KEY,
    contact VARCHAR2(255),
    params VARCHAR2(255) DEFAULT NULL,
    path VARCHAR2(255) DEFAULT NULL,
    received VARCHAR2(255) DEFAULT NULL,
    user_agent VARCHAR2(255) DEFAULT NULL,
    expires DATE DEFAULT NULL,
    callid VARCHAR2(255) DEFAULT NULL,
    CONSTRAINT contact_contact  UNIQUE (contact)
);

CREATE OR REPLACE TRIGGER contact_tr
before insert on contact FOR EACH ROW
BEGIN
  auto_id(:NEW.id);
END contact_tr;
/
BEGIN map2users('contact'); END;
/
INSERT INTO version (table_name, table_version) values ('contact','6');

CREATE TABLE impu (
    id NUMBER(10) PRIMARY KEY,
    impu VARCHAR2(64),
    barring NUMBER(10) DEFAULT 0,
    reg_state NUMBER(10) DEFAULT 0,
    ccf1 VARCHAR2(64) DEFAULT NULL,
    ccf2 VARCHAR2(64) DEFAULT NULL,
    ecf1 VARCHAR2(64) DEFAULT NULL,
    ecf2 VARCHAR2(64) DEFAULT NULL,
    ims_subscription_data BLOB,
    CONSTRAINT impu_impu  UNIQUE (impu)
);

CREATE OR REPLACE TRIGGER impu_tr
before insert on impu FOR EACH ROW
BEGIN
  auto_id(:NEW.id);
END impu_tr;
/
BEGIN map2users('impu'); END;
/
INSERT INTO version (table_name, table_version) values ('impu','6');

CREATE TABLE impu_contact (
    id NUMBER(10) PRIMARY KEY,
    impu_id NUMBER(10),
    contact_id NUMBER(10),
    CONSTRAINT impu_contact_impu_id  UNIQUE (impu_id, contact_id)
);

CREATE OR REPLACE TRIGGER impu_contact_tr
before insert on impu_contact FOR EACH ROW
BEGIN
  auto_id(:NEW.id);
END impu_contact_tr;
/
BEGIN map2users('impu_contact'); END;
/
INSERT INTO version (table_name, table_version) values ('impu_contact','6');

CREATE TABLE subscriber_scscf (
    id NUMBER(10) PRIMARY KEY,
    watcher_uri VARCHAR2(100),
    watcher_contact VARCHAR2(100),
    presentity_uri VARCHAR2(100),
    event NUMBER(10),
    expires DATE,
    version NUMBER(10),
    local_cseq NUMBER(10),
    call_id VARCHAR2(50),
    from_tag VARCHAR2(50),
    to_tag VARCHAR2(50),
    record_route CLOB,
    sockinfo_str VARCHAR2(50),
    CONSTRAINT subscriber_scscf_contact  UNIQUE (event, watcher_contact, presentity_uri)
);

CREATE OR REPLACE TRIGGER subscriber_scscf_tr
before insert on subscriber_scscf FOR EACH ROW
BEGIN
  auto_id(:NEW.id);
END subscriber_scscf_tr;
/
BEGIN map2users('subscriber_scscf'); END;
/
INSERT INTO version (table_name, table_version) values ('subscriber_scscf','6');

CREATE TABLE impu_subscriber (
    id NUMBER(10) PRIMARY KEY,
    impu_id NUMBER(10),
    subscriber_id NUMBER(10),
    CONSTRAINT impu_subscriber_impu_id  UNIQUE (impu_id, subscriber_id)
);

CREATE OR REPLACE TRIGGER impu_subscriber_tr
before insert on impu_subscriber FOR EACH ROW
BEGIN
  auto_id(:NEW.id);
END impu_subscriber_tr;
/
BEGIN map2users('impu_subscriber'); END;
/
INSERT INTO version (table_name, table_version) values ('impu_subscriber','6');
