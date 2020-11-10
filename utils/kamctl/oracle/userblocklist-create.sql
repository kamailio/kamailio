CREATE TABLE userblocklist (
    id NUMBER(10) PRIMARY KEY,
    username VARCHAR2(64) DEFAULT '',
    domain VARCHAR2(64) DEFAULT '',
    prefix VARCHAR2(64) DEFAULT '',
    allowlist NUMBER(5) DEFAULT 0 NOT NULL
);

CREATE OR REPLACE TRIGGER userblocklist_tr
before insert on userblocklist FOR EACH ROW
BEGIN
  auto_id(:NEW.id);
END userblocklist_tr;
/
BEGIN map2users('userblocklist'); END;
/
CREATE INDEX ORA_userblocklist_idx  ON userblocklist (username, domain, prefix);

INSERT INTO version (table_name, table_version) values ('userblocklist','1');

CREATE TABLE globalblocklist (
    id NUMBER(10) PRIMARY KEY,
    prefix VARCHAR2(64) DEFAULT '',
    allowlist NUMBER(5) DEFAULT 0 NOT NULL,
    description VARCHAR2(255) DEFAULT NULL
);

CREATE OR REPLACE TRIGGER globalblocklist_tr
before insert on globalblocklist FOR EACH ROW
BEGIN
  auto_id(:NEW.id);
END globalblocklist_tr;
/
BEGIN map2users('globalblocklist'); END;
/
CREATE INDEX ORA_globalblocklist_idx  ON globalblocklist (prefix);

INSERT INTO version (table_name, table_version) values ('globalblocklist','1');

