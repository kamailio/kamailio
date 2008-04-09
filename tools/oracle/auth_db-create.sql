INSERT INTO VERSION (table_name,table_version) values ('subscriber',6);
create table SUBSCRIBER
(
  ID               NUMBER(10) not null,
  USERNAME         VARCHAR2(64) default '',
  DOMAIN           VARCHAR2(64) default '',
  PASSWORD         VARCHAR2(25) default '',
  EMAIL_ADDRESS    VARCHAR2(64) default '',
  HA1              VARCHAR2(64) default '',
  HA1B             VARCHAR2(64) default '',
  RPID             VARCHAR2(64) default null
);
alter table SUBSCRIBER add constraint PK_SUBSCRIBER primary key (ID);
alter table SUBSCRIBER add constraint SUBSCRIBER_ACCOUNT_IDX unique (USERNAME,DOMAIN);
create index SUBSCRIBER_USERNAME_IDX on SUBSCRIBER (USERNAME);
create or replace trigger subscriber_tr
before insert on subscriber FOR EACH ROW
BEGIN
  auto_id(:NEW.id);
END subscriber_tr;
/
BEGIN map2users('SUBSCRIBER'); END;
/
