INSERT INTO version (table_name, table_version) values ('sip_trace','1');
CREATE TABLE sip_trace (
    id SERIAL PRIMARY KEY NOT NULL,
    date TIMESTAMP WITHOUT TIME ZONE NOT NULL DEFAULT '1900-01-01 00:00:01',
    callid VARCHAR(255) NOT NULL DEFAULT '',
    traced_user VARCHAR(128) NOT NULL DEFAULT '',
    msg TEXT NOT NULL,
    method VARCHAR(50) NOT NULL DEFAULT '',
    status VARCHAR(128) NOT NULL DEFAULT '',
    fromip VARCHAR(50) NOT NULL DEFAULT '',
    toip VARCHAR(50) NOT NULL DEFAULT '',
    fromtag VARCHAR(64) NOT NULL DEFAULT '',
    direction VARCHAR(4) NOT NULL DEFAULT ''
);

CREATE INDEX sip_trace_traced_user_idx ON sip_trace (traced_user);
CREATE INDEX sip_trace_date_idx ON sip_trace (date);
CREATE INDEX sip_trace_fromip_idx ON sip_trace (fromip);
CREATE INDEX sip_trace_callid_idx ON sip_trace (callid);

