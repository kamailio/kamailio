INSERT INTO version (table_name, table_version) values ('subscriber','6');
CREATE TABLE subscriber (
    id SERIAL PRIMARY KEY NOT NULL,
    username VARCHAR(64) NOT NULL DEFAULT '',
    domain VARCHAR(64) NOT NULL DEFAULT '',
    password VARCHAR(25) NOT NULL DEFAULT '',
    email_address VARCHAR(64) NOT NULL DEFAULT '',
    ha1 VARCHAR(64) NOT NULL DEFAULT '',
    ha1b VARCHAR(64) NOT NULL DEFAULT '',
    rpid VARCHAR(64) DEFAULT NULL,
    CONSTRAINT subscriber_account_idx UNIQUE (username, domain)
);

CREATE INDEX subscriber_username_idx ON subscriber (username);

