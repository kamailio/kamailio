INSERT INTO version (table_name, table_version) values ('presentity','2');
CREATE TABLE presentity (
    id INT(10) UNSIGNED AUTO_INCREMENT PRIMARY KEY NOT NULL,
    username VARCHAR(64) NOT NULL,
    domain VARCHAR(64) NOT NULL,
    event VARCHAR(64) NOT NULL,
    etag VARCHAR(64) NOT NULL,
    expires INT(11) NOT NULL,
    received_time INT(11) NOT NULL,
    body BLOB NOT NULL,
    UNIQUE KEY udee_presentity (username, domain, event, etag)
) ENGINE=MyISAM;

INSERT INTO version (table_name, table_version) values ('active_watchers','5');
CREATE TABLE active_watchers (
    id INT(10) UNSIGNED AUTO_INCREMENT PRIMARY KEY NOT NULL,
    pres_user VARCHAR(64) NOT NULL,
    pres_domain VARCHAR(64) NOT NULL,
    to_user VARCHAR(64) NOT NULL,
    to_domain VARCHAR(64) NOT NULL,
    from_user VARCHAR(64) NOT NULL,
    from_domain VARCHAR(64) NOT NULL,
    event VARCHAR(64) NOT NULL DEFAULT 'presence',
    event_id VARCHAR(64),
    to_tag VARCHAR(64) NOT NULL,
    from_tag VARCHAR(64) NOT NULL,
    callid VARCHAR(64) NOT NULL,
    local_cseq INT(11) NOT NULL,
    remote_cseq INT(11) NOT NULL,
    contact VARCHAR(64) NOT NULL,
    record_route TEXT,
    expires INT(11) NOT NULL,
    status VARCHAR(32) NOT NULL DEFAULT 'pending',
    version INT(11) NOT NULL DEFAULT 0,
    socket_info VARCHAR(64) NOT NULL,
    local_contact VARCHAR(128) NOT NULL,
    UNIQUE KEY tt_watchers (to_tag),
    KEY ude_active_watchers (pres_domain, pres_user, event),
    KEY exp_active_watchers (expires)
) ENGINE=MyISAM;

INSERT INTO version (table_name, table_version) values ('watchers','1');
CREATE TABLE watchers (
    id INT(10) UNSIGNED AUTO_INCREMENT PRIMARY KEY NOT NULL,
    p_user VARCHAR(64) NOT NULL,
    p_domain VARCHAR(64) NOT NULL,
    w_user VARCHAR(64) NOT NULL,
    w_domain VARCHAR(64) NOT NULL,
    subs_status VARCHAR(64) NOT NULL,
    reason VARCHAR(64),
    inserted_time INT(11) NOT NULL,
    UNIQUE KEY udud_watchers (p_user, p_domain, w_user, w_domain)
) ENGINE=MyISAM;

INSERT INTO version (table_name, table_version) values ('xcap_xml','1');
CREATE TABLE xcap_xml (
    id INT(10) UNSIGNED AUTO_INCREMENT PRIMARY KEY NOT NULL,
    username VARCHAR(64) NOT NULL,
    domain VARCHAR(64) NOT NULL,
    xcap BLOB NOT NULL,
    doc_type INT(11) NOT NULL,
    UNIQUE KEY udd_xcap (username, domain, doc_type)
) ENGINE=MyISAM;

INSERT INTO version (table_name, table_version) values ('pua','4');
CREATE TABLE pua (
    id INT(10) UNSIGNED AUTO_INCREMENT PRIMARY KEY NOT NULL,
    pres_uri VARCHAR(128) NOT NULL,
    pres_id VARCHAR(64) NOT NULL,
    event INT(11) NOT NULL,
    expires INT(11) NOT NULL,
    flag INT(11) NOT NULL,
    etag VARCHAR(64) NOT NULL,
    tuple_id VARCHAR(64),
    watcher_uri VARCHAR(128) NOT NULL,
    call_id VARCHAR(64) NOT NULL,
    to_tag VARCHAR(64) NOT NULL,
    from_tag VARCHAR(64) NOT NULL,
    cseq INT(11) NOT NULL,
    record_route TEXT,
    contact VARCHAR(128) NOT NULL,
    version INT(11) NOT NULL
) ENGINE=MyISAM;

 