INSERT INTO VERSION (table_name,table_version) values ('pdt',1);
create table PDT
(
  ID      NUMBER(10) not null,
  SDOMAIN VARCHAR2(128) not null,
  PREFIX  VARCHAR2(32) not null,
  DOMAIN  VARCHAR2(128) default ''
);
alter table PDT add constraint PK_PDT primary key (ID);
alter table PDT add constraint PDT_SDOMAIN_PREFIX_IDX unique (SDOMAIN,PREFIX);
create or replace trigger pdt_tr
before insert on pdt FOR EACH ROW
BEGIN
  auto_id(:NEW.id);
END pdt_tr;
/
BEGIN map2users('PDT'); END;
/
