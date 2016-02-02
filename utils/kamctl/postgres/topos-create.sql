CREATE TABLE topos_d (
    id SERIAL PRIMARY KEY NOT NULL,
    rectime TIMESTAMP WITHOUT TIME ZONE NOT NULL,
    callid VARCHAR(255) DEFAULT '' NOT NULL,
    a_uuid VARCHAR(255) DEFAULT '' NOT NULL,
    b_uuid VARCHAR(255) DEFAULT '' NOT NULL,
    a_contact VARCHAR(128) DEFAULT '' NOT NULL,
    b_contact VARCHAR(128) DEFAULT '' NOT NULL,
    a_tag VARCHAR(64) NOT NULL,
    b_tag VARCHAR(64) NOT NULL,
    a_rr TEXT NOT NULL,
    b_rr TEXT NOT NULL,
    iflags INTEGER DEFAULT 0 NOT NULL,
    a_uri VARCHAR(128) NOT NULL,
    b_uri VARCHAR(128) NOT NULL,
    r_uri VARCHAR(128) NOT NULL,
    a_srcip VARCHAR(50) DEFAULT '' NOT NULL,
    b_srcip VARCHAR(50) DEFAULT '' NOT NULL
);

CREATE INDEX topos_d_rectime_idx ON topos_d (rectime);
CREATE INDEX topos_d_a_callid_idx ON topos_d (callid);

INSERT INTO version (table_name, table_version) values ('topos_d','1');

CREATE TABLE topos_t (
    id SERIAL PRIMARY KEY NOT NULL,
    rectime TIMESTAMP WITHOUT TIME ZONE NOT NULL,
    callid VARCHAR(255) DEFAULT '' NOT NULL,
    a_uuid VARCHAR(255) DEFAULT '' NOT NULL,
    b_uuid VARCHAR(255) DEFAULT '' NOT NULL,
    direction INTEGER DEFAULT 0 NOT NULL,
    x_via TEXT NOT NULL
);

CREATE INDEX topos_t_rectime_idx ON topos_t (rectime);
CREATE INDEX topos_t_a_callid_idx ON topos_t (callid);

INSERT INTO version (table_name, table_version) values ('topos_t','1');

