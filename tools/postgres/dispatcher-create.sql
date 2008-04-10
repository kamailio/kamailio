INSERT INTO version (table_name, table_version) values ('dispatcher','2');
CREATE TABLE dispatcher (
    id SERIAL PRIMARY KEY NOT NULL,
    setid INTEGER DEFAULT 0 NOT NULL,
    destination VARCHAR(192) DEFAULT '' NOT NULL,
    flags INTEGER DEFAULT 0 NOT NULL,
    description VARCHAR(64) DEFAULT '' NOT NULL
);

