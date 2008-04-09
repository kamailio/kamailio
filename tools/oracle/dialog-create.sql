INSERT INTO VERSION (table_name,table_version) values ('dialog',2);
create table DIALOG
(
  ID               NUMBER(10) not null,
  HASH_ENTRY       NUMBER(10) not null,
  HASH_ID          NUMBER(10) not null,
  CALLID           VARCHAR2(255) not null,
  FROM_URI         VARCHAR2(128) not null,
  FROM_TAG         VARCHAR2(64) not null,
  TO_URI           VARCHAR2(128) not null,
  TO_TAG           VARCHAR2(64) not null,
  CALLER_CSEQ      VARCHAR2(7) not null,
  CALLEE_CSEQ      VARCHAR2(7) not null,
  CALLER_ROUTE_SET VARCHAR2(512),
  CALLEE_ROUTE_SET VARCHAR2(512),
  CALLER_CONTACT   VARCHAR2(128) not null,
  CALLEE_CONTACT   VARCHAR2(128) not null,
  CALLER_SOCK      VARCHAR2(64) not null,
  CALLEE_SOCK      VARCHAR2(64) not null,
  STATE            NUMBER(10) not null,
  START_TIME       NUMBER(10) not null,
  TIMEOUT          NUMBER(10) not null
);
alter table DIALOG add constraint PK_DIALOG primary key (ID);
create index DIALOG_HASH_IDX on DIALOG (HASH_ENTRY,HASH_ID);
create or replace trigger dialog_tr
before insert on dialog FOR EACH ROW
BEGIN
  auto_id(:NEW.id);
END dialog_tr;
/
BEGIN map2users('DIALOG'); END;
/
