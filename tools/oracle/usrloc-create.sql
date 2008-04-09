INSERT INTO VERSION (table_name,table_version) VALUES ('location',1004);
create table LOCATION
(
  ID            NUMBER(10) not null,
  USERNAME      VARCHAR2(64) default '',
  DOMAIN        VARCHAR2(64) default null,
  CONTACT       VARCHAR2(255) default '',
  RECEIVED      VARCHAR2(128) default null,
  PATH          VARCHAR2(128) default null,
  EXPIRES       DATE default to_date('28-MAY-2020 21:32:15','dd-mm-yyyy hh24:mi:ss'),
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
alter table LOCATION add constraint PK_LOCATION primary key (ID);
create index LOCATION_ACCOUNT_CONTACT_IDX on LOCATION (USERNAME,DOMAIN,CONTACT);
create or replace trigger location_tr
before insert on location FOR EACH ROW
BEGIN
  auto_id(:NEW.id);
END location_tr;
/
BEGIN map2users('LOCATION'); END;
/
