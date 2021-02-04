CREATE TABLE matrix (
    id INTEGER PRIMARY KEY NOT NULL,
    first INTEGER NOT NULL,
    second SMALLINT NOT NULL,
    res INTEGER NOT NULL
);

CREATE INDEX matrix_matrix_idx ON matrix (first, second);

INSERT INTO version (table_name, table_version) values ('matrix','1');

