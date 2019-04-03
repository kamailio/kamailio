CREATE TABLE sca_subscriptions (
    id NUMBER(10) PRIMARY KEY,
    subscriber VARCHAR2(255),
    aor VARCHAR2(255),
    event NUMBER(10) DEFAULT 0 NOT NULL,
    expires NUMBER(10) DEFAULT 0 NOT NULL,
    state NUMBER(10) DEFAULT 0 NOT NULL,
    app_idx NUMBER(10) DEFAULT 0 NOT NULL,
    call_id VARCHAR2(255),
    from_tag VARCHAR2(128),
    to_tag VARCHAR2(128),
    record_route CLOB,
    notify_cseq NUMBER(10),
    subscribe_cseq NUMBER(10),
    server_id NUMBER(10) DEFAULT 0 NOT NULL,
    CONSTRAINT ORA_sca_subscriptions_idx  UNIQUE (subscriber, call_id, from_tag, to_tag)
);

CREATE OR REPLACE TRIGGER sca_subscriptions_tr
before insert on sca_subscriptions FOR EACH ROW
BEGIN
  auto_id(:NEW.id);
END sca_subscriptions_tr;
/
BEGIN map2users('sca_subscriptions'); END;
/
CREATE INDEX ORA_sca_expires_idx  ON sca_subscriptions (server_id, expires);
CREATE INDEX ORA_sca_subscribers_idx  ON sca_subscriptions (subscriber, event);

INSERT INTO version (table_name, table_version) values ('sca_subscriptions','2');

