INSERT INTO VERSION (table_name,table_version) values ('grp',2);
create table GRP
(
  ID            NUMBER(10) not null,
  USERNAME      VARCHAR2(64) default '',
  DOMAIN        VARCHAR2(64) default '',
  GRP           VARCHAR2(64) default '',
  LAST_MODIFIED DATE default to_date('01-JAN-1900 00:00:01','dd-mm-yyyy hh24:mi:ss')
);
alter table GRP add constraint PK_GRP primary key (ID);
alter table GRP add constraint GRP_ACCOUNT_GROUP_IDX unique (USERNAME,DOMAIN,GRP);
create or replace trigger grp_tr
before insert on grp FOR EACH ROW
BEGIN
  auto_id(:NEW.id);
END grp_tr;
/
BEGIN map2users('GRP'); END;
/

INSERT INTO VERSION (table_name,table_version) values ('re_grp',1);
create table RE_GRP
(
  ID       NUMBER(10) not null,
  REG_EXP  VARCHAR2(128) default '',
  GROUP_ID NUMBER(10) default 0
);
alter table RE_GRP add constraint PK_RE_GRP primary key (ID);
create index RE_GRP_GROUP_IDX on RE_GRP (GROUP_ID);
create or replace trigger re_grp_tr
before insert on re_grp FOR EACH ROW
BEGIN
  auto_id(:NEW.id);
END re_grp_tr;
/
BEGIN map2users('RE_GRP'); END;
/
