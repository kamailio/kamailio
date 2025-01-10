CREATE TABLE dialog_in (
    id NUMBER(10) PRIMARY KEY,
    hash_entry NUMBER(10),
    hash_id NUMBER(10),
    did VARCHAR2(45),
    callid VARCHAR2(255),
    from_uri VARCHAR2(255),
    from_tag VARCHAR2(128),
    caller_original_cseq VARCHAR2(20),
    req_uri VARCHAR2(255),
    caller_route_set VARCHAR2(512),
    caller_contact VARCHAR2(255),
    caller_sock VARCHAR2(64),
    timeout NUMBER(10) DEFAULT 0 NOT NULL,
    state NUMBER(10),
    start_time NUMBER(10),
    sflags NUMBER(10) DEFAULT 0 NOT NULL,
    toroute_name VARCHAR2(32),
    toroute_index NUMBER(10)
);

CREATE OR REPLACE TRIGGER dialog_in_tr
before insert on dialog_in FOR EACH ROW
BEGIN
  auto_id(:NEW.id);
END dialog_in_tr;
/
BEGIN map2users('dialog_in'); END;
/
CREATE INDEX dialog_in_hash_idx  ON dialog_in (hash_entry, hash_id);

INSERT INTO version (table_name, table_version) values ('dialog_in','7');

CREATE TABLE dialog_out (
    id NUMBER(10) PRIMARY KEY,
    hash_entry NUMBER(10),
    hash_id NUMBER(10),
    did VARCHAR2(45),
    to_uri VARCHAR2(255),
    to_tag VARCHAR2(128),
    caller_cseq VARCHAR2(20),
    callee_cseq VARCHAR2(20),
    callee_contact VARCHAR2(255),
    callee_route_set VARCHAR2(512),
    callee_sock VARCHAR2(64)
);

CREATE OR REPLACE TRIGGER dialog_out_tr
before insert on dialog_out FOR EACH ROW
BEGIN
  auto_id(:NEW.id);
END dialog_out_tr;
/
BEGIN map2users('dialog_out'); END;
/
CREATE INDEX dialog_out_hash_idx  ON dialog_out (hash_entry, hash_id);

INSERT INTO version (table_name, table_version) values ('dialog_out','7');

CREATE TABLE dialog_vars (
    id NUMBER(10) PRIMARY KEY,
    hash_entry NUMBER(10),
    hash_id NUMBER(10),
    dialog_key VARCHAR2(128),
    dialog_value VARCHAR2(512)
);

CREATE OR REPLACE TRIGGER dialog_vars_tr
before insert on dialog_vars FOR EACH ROW
BEGIN
  auto_id(:NEW.id);
END dialog_vars_tr;
/
BEGIN map2users('dialog_vars'); END;
/
CREATE INDEX dialog_vars_hash_idx  ON dialog_vars (hash_entry, hash_id);

INSERT INTO version (table_name, table_version) values ('dialog_vars','1');
