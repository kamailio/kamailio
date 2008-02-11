INSERT INTO version (table_name, table_version) values ('grp','2');
CREATE TABLE grp (
    id SERIAL PRIMARY KEY NOT NULL,
    username VARCHAR(64) NOT NULL DEFAULT '',
    domain VARCHAR(64) NOT NULL DEFAULT '',
    grp VARCHAR(64) NOT NULL DEFAULT '',
    last_modified TIMESTAMP WITHOUT TIME ZONE NOT NULL DEFAULT '1900-01-01 00:00:01',
    CONSTRAINT grp_account_group_idx UNIQUE (username, domain, grp)
);

INSERT INTO version (table_name, table_version) values ('re_grp','1');
CREATE TABLE re_grp (
    id SERIAL PRIMARY KEY NOT NULL,
    reg_exp VARCHAR(128) NOT NULL DEFAULT '',
    group_id INTEGER NOT NULL DEFAULT 0
);

CREATE INDEX re_grp_group_idx ON re_grp (group_id);

