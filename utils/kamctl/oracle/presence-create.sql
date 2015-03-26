INSERT INTO version (table_name, table_version) values ('presentity','4');
CREATE TABLE presentity (
    id NUMBER(10) PRIMARY KEY,
    username VARCHAR2(64),
    domain VARCHAR2(64),
    event VARCHAR2(64),
    etag VARCHAR2(64),
    expires NUMBER(10),
    received_time NUMBER(10),
    body BLOB,
    sender VARCHAR2(128),
    priority NUMBER(10) DEFAULT 0 NOT NULL,
    CONSTRAINT presentity_presentity_idx  UNIQUE (username, domain, event, etag)
);

CREATE OR REPLACE TRIGGER presentity_tr
before insert on presentity FOR EACH ROW
BEGIN
  auto_id(:NEW.id);
END presentity_tr;
/
BEGIN map2users('presentity'); END;
/
CREATE INDEX presentity_presentity_expires  ON presentity (expires);
CREATE INDEX presentity_account_idx  ON presentity (username, domain, event);

INSERT INTO version (table_name, table_version) values ('active_watchers','11');
CREATE TABLE active_watchers (
    id NUMBER(10) PRIMARY KEY,
    presentity_uri VARCHAR2(128),
    watcher_username VARCHAR2(64),
    watcher_domain VARCHAR2(64),
    to_user VARCHAR2(64),
    to_domain VARCHAR2(64),
    event VARCHAR2(64) DEFAULT 'presence',
    event_id VARCHAR2(64),
    to_tag VARCHAR2(64),
    from_tag VARCHAR2(64),
    callid VARCHAR2(255),
    local_cseq NUMBER(10),
    remote_cseq NUMBER(10),
    contact VARCHAR2(128),
    record_route CLOB,
    expires NUMBER(10),
    status NUMBER(10) DEFAULT 2 NOT NULL,
    reason VARCHAR2(64),
    version NUMBER(10) DEFAULT 0 NOT NULL,
    socket_info VARCHAR2(64),
    local_contact VARCHAR2(128),
    from_user VARCHAR2(64),
    from_domain VARCHAR2(64),
    updated NUMBER(10),
    updated_winfo NUMBER(10),
    CONSTRAINT ORA_active_watchers_idx  UNIQUE (callid, to_tag, from_tag)
);

CREATE OR REPLACE TRIGGER active_watchers_tr
before insert on active_watchers FOR EACH ROW
BEGIN
  auto_id(:NEW.id);
END active_watchers_tr;
/
BEGIN map2users('active_watchers'); END;
/
CREATE INDEX ORA_active_watchers_expires  ON active_watchers (expires);
CREATE INDEX ORA_active_watchers_pres  ON active_watchers (presentity_uri, event);
CREATE INDEX active_watchers_updated_idx  ON active_watchers (updated);
CREATE INDEX ORA_updated_winfo_idx  ON active_watchers (updated_winfo, presentity_uri);

INSERT INTO version (table_name, table_version) values ('watchers','3');
CREATE TABLE watchers (
    id NUMBER(10) PRIMARY KEY,
    presentity_uri VARCHAR2(128),
    watcher_username VARCHAR2(64),
    watcher_domain VARCHAR2(64),
    event VARCHAR2(64) DEFAULT 'presence',
    status NUMBER(10),
    reason VARCHAR2(64),
    inserted_time NUMBER(10),
    CONSTRAINT watchers_watcher_idx  UNIQUE (presentity_uri, watcher_username, watcher_domain, event)
);

CREATE OR REPLACE TRIGGER watchers_tr
before insert on watchers FOR EACH ROW
BEGIN
  auto_id(:NEW.id);
END watchers_tr;
/
BEGIN map2users('watchers'); END;
/
INSERT INTO version (table_name, table_version) values ('xcap','4');
CREATE TABLE xcap (
    id NUMBER(10) PRIMARY KEY,
    username VARCHAR2(64),
    domain VARCHAR2(64),
    doc BLOB,
    doc_type NUMBER(10),
    etag VARCHAR2(64),
    source NUMBER(10),
    doc_uri VARCHAR2(255),
    port NUMBER(10),
    CONSTRAINT xcap_doc_uri_idx  UNIQUE (doc_uri)
);

CREATE OR REPLACE TRIGGER xcap_tr
before insert on xcap FOR EACH ROW
BEGIN
  auto_id(:NEW.id);
END xcap_tr;
/
BEGIN map2users('xcap'); END;
/
CREATE INDEX xcap_account_doc_type_idx  ON xcap (username, domain, doc_type);
CREATE INDEX xcap_account_doc_type_uri_idx  ON xcap (username, domain, doc_type, doc_uri);
CREATE INDEX xcap_account_doc_uri_idx  ON xcap (username, domain, doc_uri);

INSERT INTO version (table_name, table_version) values ('pua','7');
CREATE TABLE pua (
    id NUMBER(10) PRIMARY KEY,
    pres_uri VARCHAR2(128),
    pres_id VARCHAR2(255),
    event NUMBER(10),
    expires NUMBER(10),
    desired_expires NUMBER(10),
    flag NUMBER(10),
    etag VARCHAR2(64),
    tuple_id VARCHAR2(64),
    watcher_uri VARCHAR2(128),
    call_id VARCHAR2(255),
    to_tag VARCHAR2(64),
    from_tag VARCHAR2(64),
    cseq NUMBER(10),
    record_route CLOB,
    contact VARCHAR2(128),
    remote_contact VARCHAR2(128),
    version NUMBER(10),
    extra_headers CLOB,
    CONSTRAINT pua_pua_idx  UNIQUE (etag, tuple_id, call_id, from_tag)
);

CREATE OR REPLACE TRIGGER pua_tr
before insert on pua FOR EACH ROW
BEGIN
  auto_id(:NEW.id);
END pua_tr;
/
BEGIN map2users('pua'); END;
/
CREATE INDEX pua_expires_idx  ON pua (expires);
CREATE INDEX pua_dialog1_idx  ON pua (pres_id, pres_uri);
CREATE INDEX pua_dialog2_idx  ON pua (call_id, from_tag);
CREATE INDEX pua_record_idx  ON pua (pres_id);

