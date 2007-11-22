INSERT INTO version (table_name, table_version) values ('active_sessions','1');
CREATE TABLE active_sessions (
    sid VARCHAR(32) NOT NULL DEFAULT '',
    name VARCHAR(32) NOT NULL DEFAULT '',
    val TEXT,
    changed VARCHAR(14) NOT NULL DEFAULT '',
    CONSTRAINT active_sessions_sw_as_idx PRIMARY KEY (name, sid)
);

CREATE INDEX active_sessions_changed ON active_sessions (changed);

INSERT INTO version (table_name, table_version) values ('pending','6');
CREATE TABLE pending (
    id SERIAL PRIMARY KEY NOT NULL,
    phplib_id VARCHAR(32) NOT NULL DEFAULT '',
    username VARCHAR(64) NOT NULL DEFAULT '',
    domain VARCHAR(64) NOT NULL DEFAULT '',
    password VARCHAR(25) NOT NULL DEFAULT '',
    firstname VARCHAR(25) NOT NULL DEFAULT '',
    lastname VARCHAR(45) NOT NULL DEFAULT '',
    phone VARCHAR(15) NOT NULL DEFAULT '',
    email_address VARCHAR(50) NOT NULL DEFAULT '',
    datetime_created TIMESTAMP WITHOUT TIME ZONE NOT NULL DEFAULT '1900-01-01 00:00:01',
    datetime_modified TIMESTAMP WITHOUT TIME ZONE NOT NULL DEFAULT '1900-01-01 00:00:01',
    confirmation VARCHAR(64) NOT NULL DEFAULT '',
    flag VARCHAR(1) NOT NULL DEFAULT 'o',
    sendnotification VARCHAR(50) NOT NULL DEFAULT '',
    greeting VARCHAR(50) NOT NULL DEFAULT '',
    ha1 VARCHAR(64) NOT NULL DEFAULT '',
    ha1b VARCHAR(64) NOT NULL DEFAULT '',
    allow_find VARCHAR(1) NOT NULL DEFAULT 0,
    timezone VARCHAR(64) DEFAULT NULL,
    rpid VARCHAR(64) DEFAULT NULL,
    CONSTRAINT pending_sw_user_id UNIQUE (username, domain),
    CONSTRAINT pending_phplib_id UNIQUE (phplib_id)
);

CREATE INDEX pending_username_id ON pending (username);

INSERT INTO version (table_name, table_version) values ('phonebook','1');
CREATE TABLE phonebook (
    id SERIAL PRIMARY KEY NOT NULL,
    username VARCHAR(64) NOT NULL DEFAULT '',
    domain VARCHAR(64) NOT NULL DEFAULT '',
    fname VARCHAR(32) NOT NULL DEFAULT '',
    lname VARCHAR(32) NOT NULL DEFAULT '',
    sip_uri VARCHAR(64) NOT NULL DEFAULT ''
);

INSERT INTO version (table_name, table_version) values ('usr_preferences_types','1');
CREATE TABLE usr_preferences_types (
    att_name VARCHAR(32) PRIMARY KEY NOT NULL DEFAULT '',
    att_rich VARCHAR(32) NOT NULL DEFAULT 'string',
    att_raw_type INTEGER NOT NULL DEFAULT 2,
    att_type_spec TEXT NOT NULL,
    default_value VARCHAR(100) NOT NULL DEFAULT ''
);

INSERT INTO version (table_name, table_version) values ('server_monitoring','1');
CREATE TABLE server_monitoring (
    id INTEGER NOT NULL DEFAULT 0,
    time TIMESTAMP WITHOUT TIME ZONE NOT NULL DEFAULT '1900-01-01 00:00:01',
    param VARCHAR(32) NOT NULL DEFAULT '',
    value INTEGER NOT NULL DEFAULT 0,
    increment INTEGER NOT NULL DEFAULT 0,
    CONSTRAINT server_monitoring_sw_sm_idx PRIMARY KEY (id, param)
);

INSERT INTO version (table_name, table_version) values ('server_monitoring_agg','1');
CREATE TABLE server_monitoring_agg (
    param VARCHAR(32) PRIMARY KEY NOT NULL DEFAULT '',
    s_value INTEGER NOT NULL DEFAULT 0,
    s_increment INTEGER NOT NULL DEFAULT 0,
    last_aggregated_increment INTEGER NOT NULL DEFAULT 0,
    av REAL NOT NULL DEFAULT 0,
    mv INTEGER NOT NULL DEFAULT 0,
    ad REAL NOT NULL DEFAULT 0,
    lv INTEGER NOT NULL DEFAULT 0,
    min_val INTEGER NOT NULL DEFAULT 0,
    max_val INTEGER NOT NULL DEFAULT 0,
    min_inc INTEGER NOT NULL DEFAULT 0,
    max_inc INTEGER NOT NULL DEFAULT 0,
    lastupdate TIMESTAMP WITHOUT TIME ZONE NOT NULL DEFAULT '1900-01-01 00:00:01'
);

INSERT INTO version (table_name, table_version) values ('admin_privileges','1');
CREATE TABLE admin_privileges (
    username VARCHAR(64) NOT NULL DEFAULT '',
    domain VARCHAR(64) NOT NULL DEFAULT '',
    priv_name VARCHAR(64) NOT NULL DEFAULT '',
    priv_value VARCHAR(64) NOT NULL DEFAULT '',
    CONSTRAINT admin_privileges_sw_ap_idx PRIMARY KEY (username, priv_name, priv_value, domain)
);

