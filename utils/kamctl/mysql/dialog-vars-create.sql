INSERT INTO version (table_name, table_version) values ('dialog_vars','1');
CREATE TABLE dialog_vars (
    id INT(10) UNSIGNED AUTO_INCREMENT PRIMARY KEY NOT NULL,
    hash_entry INT(10) UNSIGNED NOT NULL,
    hash_id INT(10) UNSIGNED NOT NULL,
    dialog_key VARCHAR(128) NOT NULL,
    dialog_value VARCHAR(512) NOT NULL
) ENGINE=MyISAM;

CREATE INDEX hash_idx ON dialog_vars (hash_entry, hash_id);

