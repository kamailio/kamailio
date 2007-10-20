INSERT INTO version (table_name, table_version) values ('silo','5');
CREATE TABLE silo (
    id INT(10) UNSIGNED AUTO_INCREMENT PRIMARY KEY NOT NULL,
    src_addr VARCHAR(128) NOT NULL DEFAULT '',
    dst_addr VARCHAR(128) NOT NULL DEFAULT '',
    username VARCHAR(64) NOT NULL DEFAULT '',
    domain VARCHAR(64) NOT NULL DEFAULT '',
    inc_time INT NOT NULL DEFAULT 0,
    exp_time INT NOT NULL DEFAULT 0,
    snd_time INT NOT NULL DEFAULT 0,
    ctype VARCHAR(32) NOT NULL DEFAULT 'text/plain',
    body BLOB NOT NULL DEFAULT '',
    KEY account_idx (username, domain)
) ENGINE=MyISAM;

