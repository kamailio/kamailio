INSERT INTO version (table_name, table_version) values ('uid_user_attrs','3');
CREATE TABLE uid_user_attrs (
    id INTEGER PRIMARY KEY NOT NULL,
    uid VARCHAR(64) NOT NULL,
    name VARCHAR(32) NOT NULL,
    value VARCHAR(128),
    type INTEGER DEFAULT 0 NOT NULL,
    flags INTEGER DEFAULT 0 NOT NULL,
    CONSTRAINT uid_user_attrs_userattrs_idx UNIQUE (uid, name, value)
);

