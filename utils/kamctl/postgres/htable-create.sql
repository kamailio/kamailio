INSERT INTO version (table_name, table_version) values ('htable','2');
CREATE TABLE htable (
    id SERIAL PRIMARY KEY NOT NULL,
    key_name VARCHAR(64) DEFAULT '' NOT NULL,
    key_type INTEGER DEFAULT 0 NOT NULL,
    value_type INTEGER DEFAULT 0 NOT NULL,
    key_value VARCHAR(128) DEFAULT '' NOT NULL,
    expires INTEGER DEFAULT 0 NOT NULL
);

