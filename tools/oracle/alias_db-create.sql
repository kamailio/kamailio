INSERT INTO VERSION (table_name,table_version) values ('dbaliases',1);
create table DBALIASES
(
  ID             NUMBER(10) not null,
  ALIAS_USERNAME VARCHAR2(64) DEFAULT '',
  ALIAS_DOMAIN   VARCHAR2(64) DEFAULT '',
  USERNAME       VARCHAR2(64) DEFAULT '',
  DOMAIN         VARCHAR2(64) DEFAULT ''
);
alter table DBALIASES add constraint PK_DBALIASES primary key (ID);
alter table DBALIASES add constraint DBALIASES_ALIAS_IDX unique (ALIAS_USERNAME,ALIAS_DOMAIN);
create index DBALIASES_TARGET_IDX on DBALIASES (USERNAME,DOMAIN);
create or replace trigger dbaliases_tr
before insert on dbaliases FOR EACH ROW
BEGIN
  auto_id(:NEW.id);
END dbaliases_tr;
/
BEGIN map2users('DBALIASES'); END;
/
