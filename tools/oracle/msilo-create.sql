INSERT INTO VERSION (table_name,table_version) values ('silo',5);
create table SILO
(
  ID       NUMBER(10) not null,
  SRC_ADDR VARCHAR2(128) default '',
  DST_ADDR VARCHAR2(128) default '',
  USERNAME VARCHAR2(64) default '',
  DOMAIN   VARCHAR2(64) default '',
  INC_TIME NUMBER(10) default 0,
  EXP_TIME NUMBER(10) default 0,
  SND_TIME NUMBER(10) default 0,
  CTYPE    VARCHAR2(32) default 'text/plain',
  BODY     VARCHAR2(4000) default ''
);
alter table SILO add constraint PK_SILO primary key (ID);
create index SILO_ACCOUNT_IDX on SILO (USERNAME, DOMAIN);
create or replace trigger silo_tr
before insert on silo FOR EACH ROW
BEGIN
  auto_id(:NEW.id);
END silo_tr;
/
BEGIN map2users('SILO'); END;
/
