INSERT INTO VERSION (table_name,table_version) values ('trusted',4);
create table TRUSTED
(
  ID           NUMBER(10) not null,
  SRC_IP       VARCHAR2(50) not null,
  PROTO        VARCHAR2(4) not null,
  FROM_PATTERN VARCHAR2(64) default null,
  TAG          VARCHAR2(32)
);
alter table TRUSTED add constraint PK_TRUSTED primary key (ID);
create index TRUSTED_PEER_IDX on TRUSTED (SRC_IP);
create or replace trigger TRUSTED_tr
before insert on TRUSTED FOR EACH ROW
BEGIN
  auto_id(:NEW.id);
END TRUSTED_tr;
/
BEGIN map2users('TRUSTED'); END;
/

INSERT INTO VERSION (table_name,table_version) values ('address',3);
create table ADDRESS
(
  ID      NUMBER(10) not null,
  GRP     NUMBER(5) default 0,
  IP_ADDR VARCHAR2(15) not null,
  MASK    NUMBER(5) default 32,
  PORT    NUMBER(5) default 0
);
alter table ADDRESS add constraint PK_ADDRESS primary key (ID);
create or replace trigger address_tr
before insert on address FOR EACH ROW
BEGIN
  auto_id(:NEW.id);
END address_tr;
/
BEGIN map2users('ADDRESS'); END;
/
