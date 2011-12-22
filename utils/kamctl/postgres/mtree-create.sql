INSERT INTO version (table_name, table_version) values ('mtree','1');
CREATE TABLE mtree (
    id SERIAL PRIMARY KEY NOT NULL,
    tprefix VARCHAR(32) DEFAULT '' NOT NULL,
    tvalue VARCHAR(128) DEFAULT '' NOT NULL,
    CONSTRAINT mtree_tprefix_idx UNIQUE (tprefix)
);

INSERT INTO version (table_name, table_version) values ('mtrees','2');
CREATE TABLE mtrees (
    id SERIAL PRIMARY KEY NOT NULL,
    tname VARCHAR(128) DEFAULT '' NOT NULL,
    tprefix VARCHAR(32) DEFAULT '' NOT NULL,
    tvalue VARCHAR(128) DEFAULT '' NOT NULL,
    CONSTRAINT mtrees_tname_tprefix_tvalue_idx UNIQUE (tname, tprefix, tvalue)
);

