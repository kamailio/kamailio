INSERT INTO version (table_name, table_version) values ('acc','4');
CREATE TABLE acc (
    id SERIAL PRIMARY KEY NOT NULL,
    method VARCHAR(16) NOT NULL DEFAULT '',
    from_tag VARCHAR(64) NOT NULL DEFAULT '',
    to_tag VARCHAR(64) NOT NULL DEFAULT '',
    callid VARCHAR(64) NOT NULL DEFAULT '',
    sip_code VARCHAR(3) NOT NULL DEFAULT '',
    sip_reason VARCHAR(32) NOT NULL DEFAULT '',
    time TIMESTAMP WITHOUT TIME ZONE NOT NULL
);

CREATE INDEX acc_callid_idx ON acc (callid);

INSERT INTO version (table_name, table_version) values ('missed_calls','3');
CREATE TABLE missed_calls (
    id SERIAL PRIMARY KEY NOT NULL,
    method VARCHAR(16) NOT NULL DEFAULT '',
    from_tag VARCHAR(64) NOT NULL DEFAULT '',
    to_tag VARCHAR(64) NOT NULL DEFAULT '',
    callid VARCHAR(64) NOT NULL DEFAULT '',
    sip_code VARCHAR(3) NOT NULL DEFAULT '',
    sip_reason VARCHAR(32) NOT NULL DEFAULT '',
    time TIMESTAMP WITHOUT TIME ZONE NOT NULL
);

CREATE INDEX missed_calls_callid_idx ON missed_calls (callid);

