INSERT INTO VERSION (table_name,table_version) values ('domainpolicy',2);
create table DOMAINPOLICY
(
  ID          NUMBER(10) not null,
  RULE        VARCHAR2(255) not null,
  TYPE        VARCHAR2(255) not null,
  ATT         VARCHAR2(255),
  VAL         VARCHAR2(128),
  DESCRIPTION VARCHAR2(255) not null
);
alter table DOMAINPOLICY add constraint PK_DOMAINPOLICY primary key (ID);
alter table DOMAINPOLICY add constraint DOMAINPOLICY_RAV_IDX unique (RULE,ATT,VAL);
create index DOMAINPOLICY_RULE_IDX on DOMAINPOLICY (RULE);
create or replace trigger domainpolicy_tr
before insert on domainpolicy FOR EACH ROW
BEGIN
  auto_id(:NEW.id);
END domainpolicy_tr;
/
BEGIN map2users('DOMAINPOLICY'); END;
/
