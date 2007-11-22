INSERT INTO version (table_name, table_version) values ('usr_preferences','2');
CREATE TABLE usr_preferences (
    id SERIAL PRIMARY KEY NOT NULL,
    uuid VARCHAR(64) NOT NULL DEFAULT '',
    username VARCHAR(128) NOT NULL DEFAULT 0,
    domain VARCHAR(64) NOT NULL DEFAULT '',
    attribute VARCHAR(32) NOT NULL DEFAULT '',
    type INTEGER NOT NULL DEFAULT 0,
    value VARCHAR(128) NOT NULL DEFAULT '',
    last_modified TIMESTAMP WITHOUT TIME ZONE NOT NULL DEFAULT '1900-01-01 00:00:01'
);

CREATE INDEX usr_preferences_ua_idx ON usr_preferences (uuid, attribute);
CREATE INDEX usr_preferences_uda_idx ON usr_preferences (username, domain, attribute);

