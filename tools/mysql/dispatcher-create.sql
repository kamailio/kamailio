INSERT INTO version (table_name, table_version) values ('dispatcher','1');
CREATE TABLE dispatcher (
    id INT(10) UNSIGNED AUTO_INCREMENT PRIMARY KEY NOT NULL,
    setid INT NOT NULL DEFAULT 0,
    destination VARCHAR(192) NOT NULL DEFAULT '',
    description VARCHAR(64) NOT NULL DEFAULT ''
) ENGINE=MyISAM;

