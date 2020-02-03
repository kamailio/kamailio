CREATE TABLE topos_d (
    id NUMBER(10) PRIMARY KEY,
    rectime DATE,
    s_method VARCHAR2(64) DEFAULT '',
    s_cseq VARCHAR2(64) DEFAULT '',
    a_callid VARCHAR2(255) DEFAULT '',
    a_uuid VARCHAR2(255) DEFAULT '',
    b_uuid VARCHAR2(255) DEFAULT '',
    a_contact VARCHAR2(512) DEFAULT '',
    b_contact VARCHAR2(512) DEFAULT '',
    as_contact VARCHAR2(512) DEFAULT '',
    bs_contact VARCHAR2(512) DEFAULT '',
    a_tag VARCHAR2(255) DEFAULT '',
    b_tag VARCHAR2(255) DEFAULT '',
    a_rr CLOB,
    b_rr CLOB,
    s_rr CLOB,
    iflags NUMBER(10) DEFAULT 0 NOT NULL,
    a_uri VARCHAR2(255) DEFAULT '',
    b_uri VARCHAR2(255) DEFAULT '',
    r_uri VARCHAR2(255) DEFAULT '',
    a_srcaddr VARCHAR2(128) DEFAULT '',
    b_srcaddr VARCHAR2(128) DEFAULT '',
    a_socket VARCHAR2(128) DEFAULT '',
    b_socket VARCHAR2(128) DEFAULT ''
);

CREATE OR REPLACE TRIGGER topos_d_tr
before insert on topos_d FOR EACH ROW
BEGIN
  auto_id(:NEW.id);
END topos_d_tr;
/
BEGIN map2users('topos_d'); END;
/
CREATE INDEX topos_d_rectime_idx  ON topos_d (rectime);
CREATE INDEX topos_d_a_callid_idx  ON topos_d (a_callid);
CREATE INDEX topos_d_a_uuid_idx  ON topos_d (a_uuid);
CREATE INDEX topos_d_b_uuid_idx  ON topos_d (b_uuid);

INSERT INTO version (table_name, table_version) values ('topos_d','1');

CREATE TABLE topos_t (
    id NUMBER(10) PRIMARY KEY,
    rectime DATE,
    s_method VARCHAR2(64) DEFAULT '',
    s_cseq VARCHAR2(64) DEFAULT '',
    a_callid VARCHAR2(255) DEFAULT '',
    a_uuid VARCHAR2(255) DEFAULT '',
    b_uuid VARCHAR2(255) DEFAULT '',
    direction NUMBER(10) DEFAULT 0 NOT NULL,
    x_via CLOB,
    x_vbranch VARCHAR2(255) DEFAULT '',
    x_rr CLOB,
    y_rr CLOB,
    s_rr CLOB,
    x_uri VARCHAR2(255) DEFAULT '',
    a_contact VARCHAR2(512) DEFAULT '',
    b_contact VARCHAR2(512) DEFAULT '',
    as_contact VARCHAR2(512) DEFAULT '',
    bs_contact VARCHAR2(512) DEFAULT '',
    x_tag VARCHAR2(255) DEFAULT '',
    a_tag VARCHAR2(255) DEFAULT '',
    b_tag VARCHAR2(255) DEFAULT '',
    a_srcaddr VARCHAR2(255) DEFAULT '',
    b_srcaddr VARCHAR2(255) DEFAULT '',
    a_socket VARCHAR2(128) DEFAULT '',
    b_socket VARCHAR2(128) DEFAULT ''
);

CREATE OR REPLACE TRIGGER topos_t_tr
before insert on topos_t FOR EACH ROW
BEGIN
  auto_id(:NEW.id);
END topos_t_tr;
/
BEGIN map2users('topos_t'); END;
/
CREATE INDEX topos_t_rectime_idx  ON topos_t (rectime);
CREATE INDEX topos_t_a_callid_idx  ON topos_t (a_callid);
CREATE INDEX topos_t_x_vbranch_idx  ON topos_t (x_vbranch);
CREATE INDEX topos_t_a_uuid_idx  ON topos_t (a_uuid);

INSERT INTO version (table_name, table_version) values ('topos_t','1');

