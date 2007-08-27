INSERT INTO version (table_name, table_version) values ('dispatcher','1');
CREATE TABLE dispatcher (
    id SERIAL PRIMARY KEY NOT NULL,
    setid INTEGER NOT NULL DEFAULT 0,
    destination VARCHAR(192) NOT NULL DEFAULT '',
    description VARCHAR(64) NOT NULL DEFAULT ''
);

