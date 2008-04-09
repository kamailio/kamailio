INSERT INTO VERSION (table_name,table_version) values ('usr_preferences',2);
create table USR_PREFERENCES
(
  ID            NUMBER(10) not null,
  UUID          VARCHAR2(64) default '',
  USERNAME      VARCHAR2(128) default 0,
  DOMAIN        VARCHAR2(64) default '',
  ATTRIBUTE     VARCHAR2(32) default '',
  TYPE          NUMBER(10) default 0,
  VALUE         VARCHAR2(128) default '',
  LAST_MODIFIED DATE default to_date('01-JAN-1900 00:00:01','dd-mm-yyyy hh24:mi:ss')
);
alter table USR_PREFERENCES add constraint PK_USR_PREFERENCES primary key (ID);
create index USR_PREFERENCES_UA_IDX on USR_PREFERENCES (UUID,ATTRIBUTE);
create index USR_PREFERENCES_UDA_IDX on USR_PREFERENCES (USERNAME,DOMAIN, ATTRIBUTE);
create or replace trigger usr_preferences_tr
before insert on usr_preferences FOR EACH ROW
BEGIN
  auto_id(:NEW.id);
END usr_preferences_tr;
/
BEGIN map2users('USR_PREFERENCES'); END;
/
