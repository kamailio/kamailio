INSERT INTO version (table_name, table_version) values ('rls_presentity','1');
CREATE TABLE rls_presentity (
    id NUMBER(10) PRIMARY KEY,
    rlsubs_did VARCHAR2(255),
    resource_uri VARCHAR2(128),
    content_type VARCHAR2(255),
    presence_state BLOB,
    expires NUMBER(10),
    updated NUMBER(10),
    auth_state NUMBER(10),
    reason VARCHAR2(64),
    CONSTRAINT ORA_rls_presentity_idx  UNIQUE (rlsubs_did, resource_uri)
);

CREATE OR REPLACE TRIGGER rls_presentity_tr
before insert on rls_presentity FOR EACH ROW
BEGIN
  auto_id(:NEW.id);
END rls_presentity_tr;
/
BEGIN map2users('rls_presentity'); END;
/
CREATE INDEX rls_presentity_rlsubs_idx  ON rls_presentity (rlsubs_did);
CREATE INDEX rls_presentity_updated_idx  ON rls_presentity (updated);
CREATE INDEX rls_presentity_expires_idx  ON rls_presentity (expires);

INSERT INTO version (table_name, table_version) values ('rls_watchers','3');
CREATE TABLE rls_watchers (
    id NUMBER(10) PRIMARY KEY,
    presentity_uri VARCHAR2(128),
    to_user VARCHAR2(64),
    to_domain VARCHAR2(64),
    watcher_username VARCHAR2(64),
    watcher_domain VARCHAR2(64),
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
    CONSTRAINT rls_watchers_rls_watcher_idx  UNIQUE (callid, to_tag, from_tag)
);

CREATE OR REPLACE TRIGGER rls_watchers_tr
before insert on rls_watchers FOR EACH ROW
BEGIN
  auto_id(:NEW.id);
END rls_watchers_tr;
/
BEGIN map2users('rls_watchers'); END;
/
CREATE INDEX ORA_rls_watchers_update  ON rls_watchers (watcher_username, watcher_domain, event);
CREATE INDEX ORA_rls_watchers_expires  ON rls_watchers (expires);
CREATE INDEX rls_watchers_updated_idx  ON rls_watchers (updated);

