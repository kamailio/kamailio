CREATE TABLE version (
    table_name VARCHAR2(32),
    table_version NUMBER(10) DEFAULT 0 NOT NULL,
    CONSTRAINT version_table_name_idx  UNIQUE (table_name)
);

BEGIN map2users('version'); END;
/
INSERT INTO version (table_name, table_version) values ('version','1');

