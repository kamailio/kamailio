INSERT INTO version (table_name, table_version) values ('speed_dial','2');
CREATE TABLE speed_dial (
    id SERIAL PRIMARY KEY NOT NULL,
    username VARCHAR(64) NOT NULL DEFAULT '',
    domain VARCHAR(64) NOT NULL DEFAULT '',
    sd_username VARCHAR(64) NOT NULL DEFAULT '',
    sd_domain VARCHAR(64) NOT NULL DEFAULT '',
    new_uri VARCHAR(128) NOT NULL DEFAULT '',
    fname VARCHAR(64) NOT NULL DEFAULT '',
    lname VARCHAR(64) NOT NULL DEFAULT '',
    description VARCHAR(64) NOT NULL DEFAULT '',
    CONSTRAINT speed_dial_speed_dial_idx UNIQUE (username, domain, sd_domain, sd_username)
);

