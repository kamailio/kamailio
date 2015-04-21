INSERT INTO version (table_name, table_version) values ('acc','5');
CREATE TABLE acc (
    id NUMBER(10) PRIMARY KEY,
    method VARCHAR2(16) DEFAULT '',
    from_tag VARCHAR2(64) DEFAULT '',
    to_tag VARCHAR2(64) DEFAULT '',
    callid VARCHAR2(255) DEFAULT '',
    sip_code VARCHAR2(3) DEFAULT '',
    sip_reason VARCHAR2(128) DEFAULT '',
    time DATE
);

CREATE OR REPLACE TRIGGER acc_tr
before insert on acc FOR EACH ROW
BEGIN
  auto_id(:NEW.id);
END acc_tr;
/
BEGIN map2users('acc'); END;
/
CREATE INDEX acc_callid_idx  ON acc (callid);

INSERT INTO version (table_name, table_version) values ('acc_cdrs','2');
CREATE TABLE acc_cdrs (
    id NUMBER(10) PRIMARY KEY,
    start_time DATE DEFAULT '2000-01-01 00:00:00',
    end_time DATE DEFAULT '2000-01-01 00:00:00',
    duration NUMBER(10,3) DEFAULT 0 NOT NULL
);

CREATE OR REPLACE TRIGGER acc_cdrs_tr
before insert on acc_cdrs FOR EACH ROW
BEGIN
  auto_id(:NEW.id);
END acc_cdrs_tr;
/
BEGIN map2users('acc_cdrs'); END;
/
CREATE INDEX acc_cdrs_start_time_idx  ON acc_cdrs (start_time);

INSERT INTO version (table_name, table_version) values ('missed_calls','4');
CREATE TABLE missed_calls (
    id NUMBER(10) PRIMARY KEY,
    method VARCHAR2(16) DEFAULT '',
    from_tag VARCHAR2(64) DEFAULT '',
    to_tag VARCHAR2(64) DEFAULT '',
    callid VARCHAR2(255) DEFAULT '',
    sip_code VARCHAR2(3) DEFAULT '',
    sip_reason VARCHAR2(128) DEFAULT '',
    time DATE
);

CREATE OR REPLACE TRIGGER missed_calls_tr
before insert on missed_calls FOR EACH ROW
BEGIN
  auto_id(:NEW.id);
END missed_calls_tr;
/
BEGIN map2users('missed_calls'); END;
/
CREATE INDEX missed_calls_callid_idx  ON missed_calls (callid);

