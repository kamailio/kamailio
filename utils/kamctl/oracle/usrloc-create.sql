INSERT INTO version (table_name, table_version) values ('location','8');
CREATE TABLE location (
    id NUMBER(10) PRIMARY KEY,
    ruid VARCHAR2(64) DEFAULT '',
    username VARCHAR2(64) DEFAULT '',
    domain VARCHAR2(64) DEFAULT NULL,
    contact VARCHAR2(255) DEFAULT '',
    received VARCHAR2(128) DEFAULT NULL,
    path VARCHAR2(512) DEFAULT NULL,
    expires DATE DEFAULT to_date('2030-05-28 21:32:15','yyyy-mm-dd hh24:mi:ss'),
    q NUMBER(10,2) DEFAULT 1.0 NOT NULL,
    callid VARCHAR2(255) DEFAULT 'Default-Call-ID',
    cseq NUMBER(10) DEFAULT 1 NOT NULL,
    last_modified DATE DEFAULT to_date('1900-01-01 00:00:01','yyyy-mm-dd hh24:mi:ss'),
    flags NUMBER(10) DEFAULT 0 NOT NULL,
    cflags NUMBER(10) DEFAULT 0 NOT NULL,
    user_agent VARCHAR2(255) DEFAULT '',
    socket VARCHAR2(64) DEFAULT NULL,
    methods NUMBER(10) DEFAULT NULL,
    instance VARCHAR2(255) DEFAULT NULL,
    reg_id NUMBER(10) DEFAULT 0 NOT NULL,
    server_id NUMBER(10) DEFAULT 0 NOT NULL,
    connection_id NUMBER(10) DEFAULT 0 NOT NULL,
    keepalive NUMBER(10) DEFAULT 0 NOT NULL,
    partition NUMBER(10) DEFAULT 0 NOT NULL,
    CONSTRAINT location_ruid_idx  UNIQUE (ruid)
);

CREATE OR REPLACE TRIGGER location_tr
before insert on location FOR EACH ROW
BEGIN
  auto_id(:NEW.id);
END location_tr;
/
BEGIN map2users('location'); END;
/
CREATE INDEX location_account_contact_idx  ON location (username, domain, contact);
CREATE INDEX location_expires_idx  ON location (expires);

INSERT INTO version (table_name, table_version) values ('location_attrs','1');
CREATE TABLE location_attrs (
    id NUMBER(10) PRIMARY KEY,
    ruid VARCHAR2(64) DEFAULT '',
    username VARCHAR2(64) DEFAULT '',
    domain VARCHAR2(64) DEFAULT NULL,
    aname VARCHAR2(64) DEFAULT '',
    atype NUMBER(10) DEFAULT 0 NOT NULL,
    avalue VARCHAR2(255) DEFAULT '',
    last_modified DATE DEFAULT to_date('1900-01-01 00:00:01','yyyy-mm-dd hh24:mi:ss')
);

CREATE OR REPLACE TRIGGER location_attrs_tr
before insert on location_attrs FOR EACH ROW
BEGIN
  auto_id(:NEW.id);
END location_attrs_tr;
/
BEGIN map2users('location_attrs'); END;
/
CREATE INDEX ORA_account_record_idx  ON location_attrs (username, domain, ruid);
CREATE INDEX ORA_last_modified_idx  ON location_attrs (last_modified);

