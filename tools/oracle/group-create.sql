INSERT INTO version (table_name, table_version) values ('grp','2');
CREATE TABLE grp (
    id NUMBER(10) PRIMARY KEY,
    username VARCHAR2(64) DEFAULT '',
    domain VARCHAR2(64) DEFAULT '',
    grp VARCHAR2(64) DEFAULT '',
    last_modified DATE DEFAULT to_date('1900-01-01 00:00:01','yyyy-mm-dd hh24:mi:ss'),
    CONSTRAINT grp_account_group_idx  UNIQUE (username, domain, grp)
);

CREATE OR REPLACE TRIGGER grp_tr
before insert on grp FOR EACH ROW
BEGIN
  auto_id(:NEW.id);
END grp_tr;
/
BEGIN map2users('grp'); END;
/
INSERT INTO version (table_name, table_version) values ('re_grp','1');
CREATE TABLE re_grp (
    id NUMBER(10) PRIMARY KEY,
    reg_exp VARCHAR2(128) DEFAULT '',
    group_id NUMBER(10) DEFAULT 0 NOT NULL
);

CREATE OR REPLACE TRIGGER re_grp_tr
before insert on re_grp FOR EACH ROW
BEGIN
  auto_id(:NEW.id);
END re_grp_tr;
/
BEGIN map2users('re_grp'); END;
/
CREATE INDEX re_grp_group_idx  ON re_grp (group_id);

