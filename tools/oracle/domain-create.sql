INSERT INTO VERSION (table_name,table_version) values ('domain',1);
create table DOMAIN
(
  ID            NUMBER(10) not null,
  DOMAIN        VARCHAR2(64) default '',
  LAST_MODIFIED DATE default to_date('01-JAN-1900 00:00:01','dd-mm-yyyy hh24:mi:ss')
);
alter table DOMAIN add constraint PK_DOMAIN primary key (ID);
alter table DOMAIN add constraint DOMAIN_DOMAIN_IDX unique (DOMAIN);
create or replace trigger domain_tr
before insert on domain FOR EACH ROW
BEGIN
  auto_id(:NEW.id);
END domain_tr;
/
BEGIN map2users('DOMAIN'); END;
/
