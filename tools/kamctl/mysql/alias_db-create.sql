INSERT INTO version (table_name, table_version) values ('dbaliases','1');
CREATE TABLE dbaliases (
    id INT(10) UNSIGNED AUTO_INCREMENT PRIMARY KEY NOT NULL,
    alias_username VARCHAR(64) DEFAULT '' NOT NULL,
    alias_domain VARCHAR(64) DEFAULT '' NOT NULL,
    username VARCHAR(64) DEFAULT '' NOT NULL,
    domain VARCHAR(64) DEFAULT '' NOT NULL,
    CONSTRAINT alias_idx UNIQUE (alias_username, alias_domain)
) ENGINE=MyISAM;

CREATE INDEX target_idx ON dbaliases (username, domain);

