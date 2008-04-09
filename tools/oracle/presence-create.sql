INSERT INTO VERSION (table_name,table_version) VALUES ('presentity',2);
create table PRESENTITY
(
  ID            NUMBER(10) not null,
  USERNAME      VARCHAR2(64) not null,
  DOMAIN        VARCHAR2(64) not null,
  EVENT         VARCHAR2(64) not null,
  ETAG          VARCHAR2(64) not null,
  EXPIRES       NUMBER(10) not null,
  RECEIVED_TIME NUMBER(10) not null,
  BODY          VARCHAR2(4000) not null
);
alter table PRESENTITY add constraint PK_PRESENTITY primary key (ID);
alter table PRESENTITY add constraint PRESENTITY_PRESENTITY_IDX unique (USERNAME, DOMAIN, EVENT, ETAG);
create or replace trigger presentity_tr
before insert on presentity FOR EACH ROW
BEGIN
  auto_id(:NEW.id);
END presentity_tr;
/
BEGIN map2users('PRESENTITY'); END;
/

INSERT INTO VERSION (table_name,table_version) VALUES ('active_watchers',9);
create table ACTIVE_WATCHERS
(
  ID               NUMBER(10) not null,
  PRESENTITY_URI   VARCHAR2(128) not null,
  WATCHER_USERNAME VARCHAR2(64) not null,
  WATCHER_DOMAIN   VARCHAR2(64) not null,
  TO_USER          VARCHAR2(64) not null,
  TO_DOMAIN        VARCHAR2(64) not null,
  EVENT            VARCHAR2(64) default 'presence',
  EVENT_ID         VARCHAR2(64),
  TO_TAG           VARCHAR2(64) not null,
  FROM_TAG         VARCHAR2(64) not null,
  CALLID           VARCHAR2(64) not null,
  LOCAL_CSEQ       NUMBER(10) not null,
  REMOTE_CSEQ      NUMBER(10) not null,
  CONTACT          VARCHAR2(64) not null,
  RECORD_ROUTE     VARCHAR2(1000),
  EXPIRES          NUMBER(10) not null,
  STATUS           NUMBER(10) default 2,
  REASON           VARCHAR2(64) not null,
  VERSION          NUMBER(10) default 0,
  SOCKET_INFO      VARCHAR2(64) not null,
  LOCAL_CONTACT    VARCHAR2(128) not null
);
alter table ACTIVE_WATCHERS add constraint PK_ACTIVE_WATCHERS primary key (ID);
alter table ACTIVE_WATCHERS add constraint ACTIVE_WATCHERS_IDX unique (PRESENTITY_URI,CALLID,TO_TAG,FROM_TAG);
create or replace trigger active_watchers_tr
before insert on active_watchers FOR EACH ROW
BEGIN
  auto_id(:NEW.id);
END active_watchers_tr;
/
BEGIN map2users('ACTIVE_WATCHERS'); END;
/

INSERT INTO VERSION (table_name,table_version) VALUES ('watchers',3);
create table WATCHERS
(
  ID               NUMBER(10) not null,
  PRESENTITY_URI   VARCHAR2(128) not null,
  WATCHER_USERNAME VARCHAR2(64) not null,
  WATCHER_DOMAIN   VARCHAR2(64) not null,
  EVENT            VARCHAR2(64) default 'presence',
  STATUS           NUMBER(10) not null,
  REASON           VARCHAR2(64),
  INSERTED_TIME    NUMBER(10) not null
);
alter table WATCHERS add constraint PK_WATCHERS primary key (ID);
alter table WATCHERS add constraint WATCHER_IDX unique (PRESENTITY_URI,WATCHER_USERNAME,WATCHER_DOMAIN,EVENT);
create or replace trigger watchers_tr
before insert on watchers FOR EACH ROW
BEGIN
  auto_id(:NEW.id);
END watchers_tr;
/
BEGIN map2users('WATCHERS'); END;
/

INSERT INTO VERSION (table_name,table_version) VALUES ('xcap',3);
create table XCAP
(
  ID       NUMBER(10) not null,
  USERNAME VARCHAR2(64) not null,
  DOMAIN   VARCHAR2(64) not null,
  DOC_TYPE NUMBER(10) not null,
  ETAG     VARCHAR2(64) not null,
  SOURCE   NUMBER(10) not null,
  DOC_URI  VARCHAR2(128) not null,
  PORT     NUMBER(10) not null,
  DOC      VARCHAR2(4000) not null
);
alter table XCAP add constraint PK_XCAP primary key (ID);
alter table XCAP add constraint XCAP_ACCOUNT_DOC_TYPE_IDX unique (USERNAME,DOMAIN,DOC_TYPE,DOC_URI);
create index XCAP_SOURCE_IDX on XCAP (SOURCE);
create or replace trigger xcap_tr
before insert on xcap FOR EACH ROW
BEGIN
  auto_id(:NEW.id);
END xcap_tr;
/
BEGIN map2users('XCAP'); END;
/

INSERT INTO VERSION (table_name,table_version) VALUES ('pua',5);
create table PUA
(
  ID              NUMBER(10) not null,
  PRES_URI        VARCHAR2(128) not null,
  PRES_ID         VARCHAR2(64) not null,
  EVENT           NUMBER(10) not null,
  EXPIRES         NUMBER(10) not null,
  DESIRED_EXPIRES NUMBER(10) not null,
  FLAG            NUMBER(10) not null,
  ETAG            VARCHAR2(64) not null,
  TUPLE_ID        VARCHAR2(64),
  WATCHER_URI     VARCHAR2(128) not null,
  CALL_ID         VARCHAR2(64) not null,
  TO_TAG          VARCHAR2(64) not null,
  FROM_TAG        VARCHAR2(64) not null,
  CSEQ            NUMBER(10) not null,
  RECORD_ROUTE    VARCHAR2(1000),
  CONTACT         VARCHAR2(128) not null,
  VERSION         NUMBER(10) not null,
  EXTRA_HEADERS   VARCHAR2(1000) not null
);
alter table PUA add constraint PK_PUA primary key (ID);
create or replace trigger pua_tr
before insert on pua FOR EACH ROW
BEGIN
  auto_id(:NEW.id);
END pua_tr;
/
BEGIN map2users('PUA'); END;
/
