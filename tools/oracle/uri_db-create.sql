INSERT INTO VERSION (table_name,table_version) VALUES ('uri',1);
create table URI
(
  ID            NUMBER(10) not null,
  USERNAME      VARCHAR2(64) default '',
  DOMAIN        VARCHAR2(64) default '',
  URI_USER      VARCHAR2(64) default '',
  LAST_MODIFIED DATE default to_date('01-JAN-1900 00:00:01','dd-mm-yyyy hh24:mi:ss')
);
alter table URI add constraint PK_URI primary key (ID);
alter table URI add constraint URI_ACCOUNT_IDX unique (USERNAME,DOMAIN,URI_USER);
create or replace trigger uri_tr
before insert on uri FOR EACH ROW
BEGIN
  auto_id(:NEW.id);
end uri_tr;
/
BEGIN map2users('URI'); END;
/
