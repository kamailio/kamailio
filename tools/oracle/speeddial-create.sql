INSERT INTO VERSION (table_name,table_version) VALUES ('speed_dial',1);
create table SPEED_DIAL
(
  ID          NUMBER(10) not null,
  USERNAME    VARCHAR2(64) default '',
  DOMAIN      VARCHAR2(64) default '',
  SD_USERNAME VARCHAR2(64) default '',
  SD_DOMAIN   VARCHAR2(64) default '',
  NEW_URI     VARCHAR2(128) default '',
  FNAME       VARCHAR2(64) default '',
  LNAME       VARCHAR2(64) default '',
  DESCRIPTION VARCHAR2(64) default ''
);
alter table SPEED_DIAL add constraint PK_SPEED_DIAL primary key (ID);
alter table SPEED_DIAL add constraint SPEED_DIAL_IDX unique (USERNAME,DOMAIN,SD_DOMAIN,SD_USERNAME);
create or replace trigger speed_dial_tr
before insert on speed_dial FOR EACH ROW
BEGIN
  auto_id(:NEW.id);
END speed_dial_tr;
/
BEGIN map2users('SPEED_DIAL'); END;
/
