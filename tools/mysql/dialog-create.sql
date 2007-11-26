INSERT INTO version (table_name, table_version) values ('dialog','2');
CREATE TABLE dialog (
    id INT(10) UNSIGNED AUTO_INCREMENT PRIMARY KEY NOT NULL,
    hash_entry INT(10) UNSIGNED NOT NULL,
    hash_id INT(10) UNSIGNED NOT NULL,
    callid VARCHAR(255) NOT NULL,
    from_uri VARCHAR(128) NOT NULL,
    from_tag VARCHAR(64) NOT NULL,
    to_uri VARCHAR(128) NOT NULL,
    to_tag VARCHAR(64) NOT NULL,
    caller_cseq VARCHAR(7) NOT NULL,
    callee_cseq VARCHAR(7) NOT NULL,
    caller_route_set VARCHAR(512),
    callee_route_set VARCHAR(512),
    caller_contact VARCHAR(128) NOT NULL,
    callee_contact VARCHAR(128) NOT NULL,
    caller_sock VARCHAR(64) NOT NULL,
    callee_sock VARCHAR(64) NOT NULL,
    state INT(10) UNSIGNED NOT NULL,
    start_time INT(10) UNSIGNED NOT NULL,
    timeout INT(10) UNSIGNED NOT NULL,
    KEY hash_idx (hash_entry, hash_id)
) ENGINE=MyISAM;

