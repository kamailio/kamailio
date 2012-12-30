INSERT INTO version (table_name, table_version) values ('uid_credentials','7');
CREATE TABLE uid_credentials (
    id INTEGER PRIMARY KEY NOT NULL,
    auth_username VARCHAR(64) NOT NULL,
    did VARCHAR(64) DEFAULT '_default' NOT NULL,
    realm VARCHAR(64) NOT NULL,
    password VARCHAR(28) DEFAULT '' NOT NULL,
    flags INTEGER DEFAULT 0 NOT NULL,
    ha1 VARCHAR(32) NOT NULL,
    ha1b VARCHAR(32) DEFAULT '' NOT NULL,
    uid VARCHAR(64) NOT NULL
);

CREATE INDEX uid_credentials_cred_idx ON uid_credentials (auth_username, did);
CREATE INDEX uid_credentials_uid ON uid_credentials (uid);
CREATE INDEX uid_credentials_did_idx ON uid_credentials (did);
CREATE INDEX uid_credentials_realm_idx ON uid_credentials (realm);

