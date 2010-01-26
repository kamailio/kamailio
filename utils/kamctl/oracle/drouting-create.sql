INSERT INTO version (table_name, table_version) values ('dr_gateways','3');
CREATE TABLE dr_gateways (
    gwid NUMBER(10) PRIMARY KEY,
    type NUMBER(10) DEFAULT 0 NOT NULL,
    address VARCHAR2(128),
    strip NUMBER(10) DEFAULT 0 NOT NULL,
    pri_prefix VARCHAR2(64) DEFAULT NULL,
    attrs VARCHAR2(255) DEFAULT NULL,
    description VARCHAR2(128) DEFAULT ''
);

CREATE OR REPLACE TRIGGER dr_gateways_tr
before insert on dr_gateways FOR EACH ROW
BEGIN
  auto_id(:NEW.id);
END dr_gateways_tr;
/
BEGIN map2users('dr_gateways'); END;
/
INSERT INTO version (table_name, table_version) values ('dr_rules','3');
CREATE TABLE dr_rules (
    ruleid NUMBER(10) PRIMARY KEY,
    groupid VARCHAR2(255),
    prefix VARCHAR2(64),
    timerec VARCHAR2(255),
    priority NUMBER(10) DEFAULT 0 NOT NULL,
    routeid VARCHAR2(64),
    gwlist VARCHAR2(255),
    description VARCHAR2(128) DEFAULT ''
);

CREATE OR REPLACE TRIGGER dr_rules_tr
before insert on dr_rules FOR EACH ROW
BEGIN
  auto_id(:NEW.id);
END dr_rules_tr;
/
BEGIN map2users('dr_rules'); END;
/
INSERT INTO version (table_name, table_version) values ('dr_gw_lists','1');
CREATE TABLE dr_gw_lists (
    id NUMBER(10) PRIMARY KEY,
    gwlist VARCHAR2(255),
    description VARCHAR2(128) DEFAULT ''
);

CREATE OR REPLACE TRIGGER dr_gw_lists_tr
before insert on dr_gw_lists FOR EACH ROW
BEGIN
  auto_id(:NEW.id);
END dr_gw_lists_tr;
/
BEGIN map2users('dr_gw_lists'); END;
/
INSERT INTO version (table_name, table_version) values ('dr_groups','2');
CREATE TABLE dr_groups (
    id NUMBER(10) PRIMARY KEY,
    username VARCHAR2(64),
    domain VARCHAR2(128) DEFAULT '',
    groupid NUMBER(10) DEFAULT 0 NOT NULL,
    description VARCHAR2(128) DEFAULT ''
);

CREATE OR REPLACE TRIGGER dr_groups_tr
before insert on dr_groups FOR EACH ROW
BEGIN
  auto_id(:NEW.id);
END dr_groups_tr;
/
BEGIN map2users('dr_groups'); END;
/
