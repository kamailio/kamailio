INSERT INTO version (table_name, table_version) values ('matrix','1');
CREATE TABLE matrix (
    first INTEGER NOT NULL,
    second SMALLINT NOT NULL,
    res INTEGER NOT NULL
);

CREATE INDEX matrix_matrix_idx ON matrix (first, second);

