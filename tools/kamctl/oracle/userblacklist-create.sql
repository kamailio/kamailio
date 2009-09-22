INSERT INTO version (table_name, table_version) values ('userblacklist','1');
CREATE TABLE userblacklist (
    id NUMBER(10) PRIMARY KEY,
    username VARCHAR2(64) DEFAULT '',
    domain VARCHAR2(64) DEFAULT '',
    prefix VARCHAR2(64) DEFAULT '',
    whitelist NUMBER(5) DEFAULT 0 NOT NULL
);

CREATE OR REPLACE TRIGGER userblacklist_tr
before insert on userblacklist FOR EACH ROW
BEGIN
  auto_id(:NEW.id);
END userblacklist_tr;
/
BEGIN map2users('userblacklist'); END;
/
CREATE INDEX ORA_userblacklist_idx  ON userblacklist (username, domain, prefix);

INSERT INTO version (table_name, table_version) values ('globalblacklist','1');
CREATE TABLE globalblacklist (
    id NUMBER(10) PRIMARY KEY,
    prefix VARCHAR2(64) DEFAULT '',
    whitelist NUMBER(5) DEFAULT 0 NOT NULL,
    description VARCHAR2(255) DEFAULT NULL
);

CREATE OR REPLACE TRIGGER globalblacklist_tr
before insert on globalblacklist FOR EACH ROW
BEGIN
  auto_id(:NEW.id);
END globalblacklist_tr;
/
BEGIN map2users('globalblacklist'); END;
/
CREATE INDEX ORA_globalblacklist_idx  ON globalblacklist (prefix);

