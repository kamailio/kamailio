INSERT INTO VERSION (table_name,table_version) values ('dispatcher',2);
create table DISPATCHER
(
  ID          NUMBER(10) not null,
  SETID       NUMBER(10) default 0,
  DESTINATION VARCHAR2(192) default '',
  FLAGS       NUMBER(11) default 0,
  DESCRIPTION VARCHAR2(64) default ''
);
alter table DISPATCHER add constraint PK_DISPATCHER primary key (ID);
create or replace trigger dispatcher_tr
before insert on dispatcher FOR EACH ROW
BEGIN
  auto_id(:NEW.id);
END dispatcher_tr;
/
BEGIN map2users('DISPATCHER'); END;
/
