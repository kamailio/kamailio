INSERT INTO version (table_name, table_version) values ('pdt','1');
CREATE TABLE pdt (
    id INT(10) UNSIGNED AUTO_INCREMENT PRIMARY KEY NOT NULL,
    sdomain VARCHAR(128) NOT NULL,
    prefix VARCHAR(32) NOT NULL,
    domain VARCHAR(128) NOT NULL DEFAULT '',
    UNIQUE KEY sp_pdt (sdomain, prefix)
) ENGINE=MyISAM;

 