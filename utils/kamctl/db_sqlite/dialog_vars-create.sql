INSERT INTO version (table_name, table_version) values ('dialog_vars','1');
CREATE TABLE dialog_vars (
    id INTEGER PRIMARY KEY NOT NULL,
    hash_entry INTEGER NOT NULL,
    hash_id INTEGER NOT NULL,
    dialog_key VARCHAR(128) NOT NULL,
    dialog_value VARCHAR(512) NOT NULL
) ENGINE=MyISAM;

CREATE INDEX hash_idx ON dialog_vars (hash_entry, hash_id);

