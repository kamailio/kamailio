INSERT INTO version (table_name, table_version) values ('silo','8');
CREATE TABLE silo (
    id NUMBER(10) PRIMARY KEY,
    src_addr VARCHAR2(128) DEFAULT '',
    dst_addr VARCHAR2(128) DEFAULT '',
    username VARCHAR2(64) DEFAULT '',
    domain VARCHAR2(64) DEFAULT '',
    inc_time NUMBER(10) DEFAULT 0 NOT NULL,
    exp_time NUMBER(10) DEFAULT 0 NOT NULL,
    snd_time NUMBER(10) DEFAULT 0 NOT NULL,
    ctype VARCHAR2(32) DEFAULT 'text/plain',
    body BLOB,
    extra_hdrs CLOB,
    callid VARCHAR2(128) DEFAULT '',
    status NUMBER(10) DEFAULT 0 NOT NULL
);

CREATE OR REPLACE TRIGGER silo_tr
before insert on silo FOR EACH ROW
BEGIN
  auto_id(:NEW.id);
END silo_tr;
/
BEGIN map2users('silo'); END;
/
CREATE INDEX silo_account_idx  ON silo (username, domain);

