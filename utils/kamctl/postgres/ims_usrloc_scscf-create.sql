CREATE TABLE contact (
    id SERIAL PRIMARY KEY NOT NULL,
    contact VARCHAR(255) NOT NULL,
    params VARCHAR(255) DEFAULT NULL,
    path VARCHAR(255) DEFAULT NULL,
    received VARCHAR(255) DEFAULT NULL,
    user_agent VARCHAR(255) DEFAULT NULL,
    expires TIMESTAMP WITHOUT TIME ZONE DEFAULT NULL,
    callid VARCHAR(255) DEFAULT NULL,
    CONSTRAINT contact_contact UNIQUE (contact)
);

INSERT INTO version (table_name, table_version) values ('contact','6');

CREATE TABLE impu (
    id SERIAL PRIMARY KEY NOT NULL,
    impu VARCHAR(64) NOT NULL,
    barring INTEGER DEFAULT 0,
    reg_state INTEGER DEFAULT 0,
    ccf1 VARCHAR(64) DEFAULT NULL,
    ccf2 VARCHAR(64) DEFAULT NULL,
    ecf1 VARCHAR(64) DEFAULT NULL,
    ecf2 VARCHAR(64) DEFAULT NULL,
    ims_subscription_data BYTEA,
    CONSTRAINT impu_impu UNIQUE (impu)
);

INSERT INTO version (table_name, table_version) values ('impu','6');

CREATE TABLE impu_contact (
    id SERIAL PRIMARY KEY NOT NULL,
    impu_id INTEGER NOT NULL,
    contact_id INTEGER NOT NULL,
    CONSTRAINT impu_contact_impu_id UNIQUE (impu_id, contact_id)
);

INSERT INTO version (table_name, table_version) values ('impu_contact','6');

CREATE TABLE subscriber_scscf (
    id SERIAL PRIMARY KEY NOT NULL,
    watcher_uri VARCHAR(100) NOT NULL,
    watcher_contact VARCHAR(100) NOT NULL,
    presentity_uri VARCHAR(100) NOT NULL,
    event INTEGER NOT NULL,
    expires TIMESTAMP WITHOUT TIME ZONE NOT NULL,
    version INTEGER NOT NULL,
    local_cseq INTEGER NOT NULL,
    call_id VARCHAR(50) NOT NULL,
    from_tag VARCHAR(50) NOT NULL,
    to_tag VARCHAR(50) NOT NULL,
    record_route TEXT NOT NULL,
    sockinfo_str VARCHAR(50) NOT NULL,
    CONSTRAINT subscriber_scscf_contact UNIQUE (event, watcher_contact, presentity_uri)
);

INSERT INTO version (table_name, table_version) values ('subscriber_scscf','6');

CREATE TABLE impu_subscriber (
    id SERIAL PRIMARY KEY NOT NULL,
    impu_id INTEGER NOT NULL,
    subscriber_id INTEGER NOT NULL,
    CONSTRAINT impu_subscriber_impu_id UNIQUE (impu_id, subscriber_id)
);

INSERT INTO version (table_name, table_version) values ('impu_subscriber','6');
