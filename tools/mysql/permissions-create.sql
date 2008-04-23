INSERT INTO version (table_name, table_version) values ('trusted','4');
CREATE TABLE trusted (
    id INT(10) UNSIGNED AUTO_INCREMENT PRIMARY KEY NOT NULL,
    src_ip VARCHAR(50) NOT NULL,
    proto VARCHAR(4) NOT NULL,
    from_pattern VARCHAR(64) DEFAULT NULL,
    tag VARCHAR(32)
) ENGINE=MyISAM;

CREATE INDEX peer_idx ON trusted (src_ip);

INSERT INTO version (table_name, table_version) values ('address','3');
CREATE TABLE address (
    id INT(10) UNSIGNED AUTO_INCREMENT PRIMARY KEY NOT NULL,
    grp SMALLINT(5) UNSIGNED DEFAULT 0 NOT NULL,
    ip_addr VARCHAR(15) NOT NULL,
    mask TINYINT DEFAULT 32 NOT NULL,
    port SMALLINT(5) UNSIGNED DEFAULT 0 NOT NULL
) ENGINE=MyISAM;

