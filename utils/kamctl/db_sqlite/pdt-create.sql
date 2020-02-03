CREATE TABLE pdt (
    id INTEGER PRIMARY KEY NOT NULL,
    sdomain VARCHAR(255) NOT NULL,
    prefix VARCHAR(32) NOT NULL,
    domain VARCHAR(255) DEFAULT '' NOT NULL,
    CONSTRAINT pdt_sdomain_prefix_idx UNIQUE (sdomain, prefix)
);

INSERT INTO version (table_name, table_version) values ('pdt','1');

