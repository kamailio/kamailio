INSERT INTO version (table_name, table_version) values ('mtree','1');
CREATE TABLE mtree (
    id INT(10) UNSIGNED AUTO_INCREMENT PRIMARY KEY NOT NULL,
    tprefix VARCHAR(32) DEFAULT '' NOT NULL,
    tvalue VARCHAR(128) DEFAULT '' NOT NULL,
    CONSTRAINT tprefix_idx UNIQUE (tprefix)
) ENGINE=MyISAM;

