INSERT INTO version (table_name, table_version) values ('sip_trace','1');
CREATE TABLE sip_trace (
    id INT(10) UNSIGNED AUTO_INCREMENT PRIMARY KEY NOT NULL,
    date DATETIME NOT NULL DEFAULT '1900-01-01 00:00:01',
    callid VARCHAR(255) NOT NULL DEFAULT '',
    traced_user VARCHAR(128) NOT NULL DEFAULT '',
    msg TEXT NOT NULL,
    method VARCHAR(50) NOT NULL DEFAULT '',
    status VARCHAR(128) NOT NULL DEFAULT '',
    fromip VARCHAR(50) NOT NULL DEFAULT '',
    toip VARCHAR(50) NOT NULL DEFAULT '',
    fromtag VARCHAR(64) NOT NULL DEFAULT '',
    direction VARCHAR(4) NOT NULL DEFAULT '',
    KEY traced_user_idx (traced_user),
    KEY date_idx (date),
    KEY fromip_idx (fromip),
    KEY callid_idx (callid)
) ENGINE=MyISAM;

