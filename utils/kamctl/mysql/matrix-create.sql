INSERT INTO version (table_name, table_version) values ('matrix','1');
CREATE TABLE `matrix` (
    `first` INT(10) NOT NULL,
    `second` SMALLINT(10) NOT NULL,
    `res` INT(10) NOT NULL
);

CREATE INDEX matrix_idx ON matrix (`first`, `second`);

