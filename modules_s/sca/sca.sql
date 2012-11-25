INSERT INTO version (table_name, table_version) values ('sca_subscriptions', '0');
CREATE TABLE sca_subscriptions (
    id INT(10) UNSIGNED AUTO_INCREMENT PRIMARY KEY NOT NULL,
    subscriber VARCHAR(255) NOT NULL,
    aor VARCHAR(255) NOT NULL,
    event INT(1) NOT NULL,
    expires INT(11) NOT NULL,
    state INT(1) NOT NULL,
    app_idx INT(4) NOT NULL,
    call_id VARCHAR(255) NOT NULL,
    from_tag VARCHAR(128) NOT NULL,
    to_tag VARCHAR(128) NOT NULL,
    notify_cseq INT(11) NOT NULL,
    subscribe_cseq INT(11) NOT NULL,

    CONSTRAINT sca_subscriptions_idx UNIQUE (subscriber, call_id, from_tag, to_tag)
) ENGINE=MyISAM;
