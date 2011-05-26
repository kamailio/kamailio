INSERT INTO version (table_name, table_version) values ('mtree','1');
CREATE TABLE mtree (
    id INTEGER PRIMARY KEY NOT NULL,
    tprefix VARCHAR(32) DEFAULT '' NOT NULL,
    tvalue VARCHAR(128) DEFAULT '' NOT NULL,
    CONSTRAINT mtree_tprefix_idx UNIQUE (tprefix)
);

