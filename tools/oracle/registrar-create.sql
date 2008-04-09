INSERT INTO VERSION (table_name,table_version) values ('aliases',1004);
create table ALIASES
(
  ID            NUMBER(10) not null,
  USERNAME      VARCHAR2(64) default '',
  DOMAIN        VARCHAR2(64) default null,
  CONTACT       VARCHAR2(255) default '',
  RECEIVED      VARCHAR2(128) default null,
  PATH          VARCHAR2(128) default null,
  EXPIRES       DATE default to_date('28-FEB-2020 21:32:15','dd-mm-yyyy hh24:mi:ss'),
  Q             NUMBER(10,2) default 1,
  CALLID        VARCHAR2(255) default 'Default-Call-ID',
  CSEQ          NUMBER(10) default 13,
  LAST_MODIFIED DATE default to_date('01-JAN-1900 00:00:01','dd-mm-yyyy hh24:mi:ss'),
  FLAGS         NUMBER(11) default 0,
  CFLAGS        NUMBER(11) default 0,
  USER_AGENT    VARCHAR2(255) default '',
  SOCKET        VARCHAR2(64) default null,
  METHODS       NUMBER(10) default null
);
alter table ALIASES add constraint PK_ALIASES primary key (ID);
create index ALIASES_ALIAS_IDX on ALIASES (USERNAME,DOMAIN,CONTACT);
create or replace trigger aliases_tr
before insert on aliases FOR EACH ROW
BEGIN
  auto_id(:NEW.id);
END aliases_tr;
/
BEGIN map2users('ALIASES'); END;
/
