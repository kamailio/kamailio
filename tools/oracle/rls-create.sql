INSERT INTO VERSION (table_name,table_version) VALUES ('rls_presentity',0);
create table RLS_PRESENTITY
(
  ID             NUMBER(10) not null,
  RLSUBS_DID     VARCHAR2(512) not null,
  RESOURCE_URI   VARCHAR2(128) not null,
  CONTENT_TYPE   VARCHAR2(64) not null,
  EXPIRES        NUMBER(10) not null,
  UPDATED        NUMBER(10) not null,
  AUTH_STATE     NUMBER(10) not null,
  REASON         VARCHAR2(64) not null,
  PRESENCE_STATE VARCHAR2(4000) not null
);
alter table RLS_PRESENTITY add constraint PK_RLS_PRESENTITY primary key (ID);
alter table RLS_PRESENTITY add constraint RLS_PRESENTITY_IDX unique (RLSUBS_DID,RESOURCE_URI);
alter table RLS_PRESENTITY add constraint RLS_PRESENTITY_UPDATED_IDX unique (UPDATED);
create or replace trigger rls_presentity_tr
before insert on rls_presentity FOR EACH ROW
BEGIN
  auto_id(:NEW.id);
END rls_presentity_tr;
/
BEGIN map2users('RLS_PRESENTITY'); END;
/

INSERT INTO VERSION (table_name,table_version) VALUES ('rls_watchers',1);
CREATE table RLS_WATCHERS
(
  ID               NUMBER(10) not null,
  PRESENTITY_URI   VARCHAR2(128) not null,
  TO_USER          VARCHAR2(64) not null,
  TO_DOMAIN        VARCHAR2(64) not null,
  WATCHER_USERNAME VARCHAR2(64) not null,
  WATCHER_DOMAIN   VARCHAR2(64) not null,
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
alter table RLS_WATCHERS add constraint PK_RLS_WATCHERS primary key (ID);
alter table RLS_WATCHERS add constraint RLS_WATCHERS_RLS_WATCHER_IDX unique (PRESENTITY_URI,CALLID,TO_TAG,FROM_TAG);
create or replace trigger rls_watchers_tr
before insert on rls_watchers FOR EACH ROW
BEGIN
  auto_id(:NEW.id);
END rls_watchers_tr;
/
BEGIN map2users('RLS_WATCHERS'); END;
/
