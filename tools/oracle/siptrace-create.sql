INSERT INTO VERSION (table_name,table_version) VALUES ('sip_trace',2);
create table SIP_TRACE
(
  ID          NUMBER(10) not null,
  TIME_STAMP  DATE default to_date('01-JAN-1900 00:00:01','dd-mm-yyyy hh24:mi:ss'),
  CALLID      VARCHAR2(255) default '',
  TRACED_USER VARCHAR2(128) default '',
  MSG         VARCHAR2(1000) not null,
  METHOD      VARCHAR2(50) default '',
  STATUS      VARCHAR2(128) default '',
  FROMIP      VARCHAR2(50) default '',
  TOIP        VARCHAR2(50) default '',
  FROMTAG     VARCHAR2(64) default '',
  DIRECTION   VARCHAR2(4) default ''
);
alter table SIP_TRACE add constraint PK_SIP_TRACE primary key (ID);
create index SIP_TRACE_CALLID_IDX on SIP_TRACE (CALLID);
create index SIP_TRACE_DATE_IDX on SIP_TRACE (TIME_STAMP);
create index SIP_TRACE_FROMIP_IDX on SIP_TRACE (FROMIP);
create index SIP_TRACE_TRACED_USER_IDX on SIP_TRACE (TRACED_USER);
create or replace trigger sip_trace_tr
before insert on sip_trace FOR EACH ROW
BEGIN
  auto_id(:NEW.id);
END sip_trace_tr;
/
BEGIN map2users('SIP_TRACE'); END;
/
