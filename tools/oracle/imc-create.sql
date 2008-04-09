INSERT INTO VERSION (table_name,table_version) values ('imc_rooms',1);
create table IMC_ROOMS
(
  ID     NUMBER(10) not null,
  NAME   VARCHAR2(64) not null,
  DOMAIN VARCHAR2(64) not null,
  FLAG   NUMBER(10) not null
);
alter table IMC_ROOMS add constraint PK_IMC_ROOMS primary key (ID);
alter table IMC_ROOMS add constraint IMC_ROOMS_NAME_DOMAIN_IDX unique (NAME,DOMAIN);
create or replace trigger imc_rooms_tr
before insert on imc_rooms FOR EACH ROW
BEGIN
  auto_id(:NEW.id);
END imc_rooms_tr;
/
BEGIN map2users('IMC_ROOMS'); END;
/

INSERT INTO VERSION (table_name,table_version) values ('imc_members',1);
create table IMC_MEMBERS
(
  ID       NUMBER(10) not null,
  USERNAME VARCHAR2(64) not null,
  DOMAIN   VARCHAR2(64) not null,
  ROOM     VARCHAR2(64) not null,
  FLAG     NUMBER(10) not null
);
alter table IMC_MEMBERS add constraint PK_IMC_MEMBERS primary key (ID);
alter table IMC_MEMBERS add constraint IMC_MEMBERS_ACCOUNT_ROOM_IDX unique (USERNAME,DOMAIN,ROOM);
create or replace trigger imc_members_tr
before insert on imc_members FOR EACH ROW
BEGIN
  auto_id(:NEW.id);
END imc_members_tr;
/
BEGIN map2users('IMC_MEMBERS'); END;
/
