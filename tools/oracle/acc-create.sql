INSERT INTO VERSION (table_name,table_version) values ('acc',4);
create table ACC
(
  ID         NUMBER(10) not null,
  METHOD     VARCHAR2(16) DEFAULT '',
  FROM_TAG   VARCHAR2(64) DEFAULT '',
  TO_TAG     VARCHAR2(64) DEFAULT '',
  CALLID     VARCHAR2(64) DEFAULT '',
  SIP_CODE   VARCHAR2(3) DEFAULT '',
  SIP_REASON VARCHAR2(32) DEFAULT '',
  TIME       DATE not null
);
alter table ACC add constraint PK_ACC primary key (ID);
create index ACC_CALLID_IDX on ACC (CALLID);
create or replace trigger acc_tr
before insert on acc FOR EACH ROW
BEGIN
  auto_id(:NEW.id);
END acc_tr;
/
BEGIN map2users('ACC'); END;
/

INSERT INTO VERSION (table_name,table_version) values ('missed_calls',3);
create table MISSED_CALLS
(
  ID         NUMBER(10) not null,
  METHOD     VARCHAR2(16) DEFAULT '',
  FROM_TAG   VARCHAR2(64) DEFAULT '',
  TO_TAG     VARCHAR2(64) DEFAULT '',
  CALLID     VARCHAR2(64) DEFAULT '',
  SIP_CODE   VARCHAR2(3) DEFAULT '',
  SIP_REASON VARCHAR2(32) DEFAULT '',
  TIME       DATE not null
);
alter table MISSED_CALLS add constraint PK_MISSED_CALLS primary key (ID);
create index MISSED_CALLS_CALLID_IDX on MISSED_CALLS (CALLID);
create or replace trigger missed_calls_tr
before insert on missed_calls FOR EACH ROW
BEGIN
  auto_id(:NEW.id);
END missed_calls_tr;
/
BEGIN map2users('MISSED_CALLS'); END;
/
