INSERT INTO VERSION (table_name,table_version) values ('gw',7);
create table GW
(
  ID         NUMBER(10) not null,
  GW_NAME    VARCHAR2(128) not null,
  GRP_ID     NUMBER(10) not null,
  IP_ADDR    VARCHAR2(15) not null,
  PORT       NUMBER(5),
  URI_SCHEME NUMBER(5),
  TRANSPORT  NUMBER(5),
  STRIP      NUMBER(5),
  TAG        VARCHAR2(16) default null,
  FLAGS      NUMBER(11) default 0
);
alter table GW add constraint PK_GW primary key (ID);
alter table GW add constraint GW_NAME_IDX unique (GW_NAME);
create index GW_GRP_ID_IDX on GW (GRP_ID);
create or replace trigger gw_tr
before insert on gw FOR EACH ROW
BEGIN
  auto_id(:NEW.id);
END gw_tr;
/
BEGIN map2users('GW'); END;
/

INSERT INTO VERSION (table_name,table_version) values ('lcr',2);
create table LCR
(
  ID       NUMBER(10) not null,
  PREFIX   VARCHAR2(16) default null,
  FROM_URI VARCHAR2(64) default null,
  GRP_ID   NUMBER(10) not null,
  PRIORITY NUMBER(5) not null
);
alter table LCR add constraint PK_LCR primary key (ID);
create index LCR_FROM_URI_IDX on LCR (FROM_URI);
create index LCR_GRP_ID_IDX on LCR (GRP_ID);
create index LCR_PREFIX_IDX on LCR (PREFIX);
create or replace trigger lcr_tr
before insert on lcr FOR EACH ROW
BEGIN
  auto_id(:NEW.id);
END lcr_tr;
/
BEGIN map2users('LCR'); END;
/
