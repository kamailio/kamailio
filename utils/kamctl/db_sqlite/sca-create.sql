CREATE TABLE sca_subscriptions (
    id INTEGER PRIMARY KEY NOT NULL,
    subscriber VARCHAR(255) NOT NULL,
    aor VARCHAR(255) NOT NULL,
    event INTEGER DEFAULT 0 NOT NULL,
    expires INTEGER DEFAULT 0 NOT NULL,
    state INTEGER DEFAULT 0 NOT NULL,
    app_idx INTEGER DEFAULT 0 NOT NULL,
    call_id VARCHAR(255) NOT NULL,
    from_tag VARCHAR(128) NOT NULL,
    to_tag VARCHAR(128) NOT NULL,
    record_route TEXT,
    notify_cseq INTEGER NOT NULL,
    subscribe_cseq INTEGER NOT NULL,
    server_id INTEGER DEFAULT 0 NOT NULL,
    CONSTRAINT sca_subscriptions_sca_subscriptions_idx UNIQUE (subscriber, call_id, from_tag, to_tag)
);

CREATE INDEX sca_subscriptions_sca_expires_idx ON sca_subscriptions (server_id, expires);
CREATE INDEX sca_subscriptions_sca_subscribers_idx ON sca_subscriptions (subscriber, event);

INSERT INTO version (table_name, table_version) values ('sca_subscriptions','2');

