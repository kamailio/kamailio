INSERT INTO VERSION (table_name,table_version) VALUES ('userblacklist',1);
CREATE TABLE USERBLACKLIST
(
  ID          NUMBER(10) not null,
  USERNAME    VARCHAR2(64) default '',
  DOMAIN      VARCHAR2(64) default '',
  PREFIX      VARCHAR2(64) default '',
  WHITELIST   NUMBER(1) default 0,
  DESCRIPTION VARCHAR2(64) default ''
);
alter table USERBLACKLIST add constraint PK_USERBLACKLIST primary key (ID);
create index USERBLACKLIST_IDX on USERBLACKLIST (USERNAME,DOMAIN,PREFIX);
create or replace trigger userblacklist_tr
before insert on userblacklist FOR EACH ROW
BEGIN
  auto_id(:NEW.id);
END userblacklist_tr;
/
BEGIN map2users('USERBLACKLIST'); END;
/

INSERT INTO VERSION (table_name,table_version) VALUES ('globalblacklist',1);
CREATE TABLE GLOBALBLACKLIST
(
  ID          NUMBER(10) not null,
  PREFIX      VARCHAR2(64) default '',
  WHITELIST   NUMBER(1) default 0,
  DESCRIPTION VARCHAR2(64) default ''
);
alter table GLOBALBLACKLIST add constraint PK_GLOBALBLACKLIST primary key (ID);
create index GLOBALBLACKLIST_USRBLCKLST_IDX on GLOBALBLACKLIST (PREFIX);
create or replace trigger globalblacklist_tr
before insert on globalblacklist FOR EACH ROW
BEGIN
  auto_id(:NEW.id);
END globalblacklist_tr;
/
BEGIN map2users('GLOBALBLACKLIST'); END;
/
