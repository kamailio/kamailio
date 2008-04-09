INSERT INTO VERSION (table_name,table_version) values ('cpl',2);
create table CPL
(
  ID       NUMBER(10) not null,
  USERNAME VARCHAR2(64) not null,
  DOMAIN   VARCHAR2(64) default '',
  CPL_XML  VARCHAR2(1000),
  CPL_BIN  VARCHAR2(1000)
);
alter table CPL add constraint PK_CPL primary key (ID);
alter table CPL add constraint CPL_ACCOUNT_IDX unique (USERNAME,DOMAIN);
create or replace trigger cpl_tr
before insert on cpl FOR EACH ROW
BEGIN
  auto_id(:NEW.id);
end cpl_tr;
/
BEGIN map2users('CPL'); END;
/
