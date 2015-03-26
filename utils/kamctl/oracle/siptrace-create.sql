INSERT INTO version (table_name, table_version) values ('sip_trace','4');
CREATE TABLE sip_trace (
    id NUMBER(10) PRIMARY KEY,
    time_stamp DATE DEFAULT to_date('1900-01-01 00:00:01','yyyy-mm-dd hh24:mi:ss'),
    time_us NUMBER(10) DEFAULT 0 NOT NULL,
    callid VARCHAR2(255) DEFAULT '',
    traced_user VARCHAR2(128) DEFAULT '',
    msg CLOB,
    method VARCHAR2(50) DEFAULT '',
    status VARCHAR2(128) DEFAULT '',
    fromip VARCHAR2(50) DEFAULT '',
    toip VARCHAR2(50) DEFAULT '',
    fromtag VARCHAR2(64) DEFAULT '',
    totag VARCHAR2(64) DEFAULT '',
    direction VARCHAR2(4) DEFAULT ''
);

CREATE OR REPLACE TRIGGER sip_trace_tr
before insert on sip_trace FOR EACH ROW
BEGIN
  auto_id(:NEW.id);
END sip_trace_tr;
/
BEGIN map2users('sip_trace'); END;
/
CREATE INDEX sip_trace_traced_user_idx  ON sip_trace (traced_user);
CREATE INDEX sip_trace_date_idx  ON sip_trace (time_stamp);
CREATE INDEX sip_trace_fromip_idx  ON sip_trace (fromip);
CREATE INDEX sip_trace_callid_idx  ON sip_trace (callid);

