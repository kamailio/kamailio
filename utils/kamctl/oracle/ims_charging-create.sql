CREATE TABLE ro_session (
    id NUMBER(10) PRIMARY KEY,
    hash_entry NUMBER(10),
    hash_id NUMBER(10),
    session_id VARCHAR2(100),
    dlg_hash_entry NUMBER(10),
    dlg_hash_id NUMBER(10),
    direction NUMBER(10),
    asserted_identity VARCHAR2(100),
    callee VARCHAR2(100),
    start_time DATE DEFAULT NULL,
    last_event_timestamp DATE DEFAULT NULL,
    reserved_secs NUMBER(10) DEFAULT NULL,
    valid_for NUMBER(10) DEFAULT NULL,
    state NUMBER(10) DEFAULT NULL,
    incoming_trunk_id VARCHAR2(20) DEFAULT NULL,
    outgoing_trunk_id VARCHAR2(20) DEFAULT NULL,
    rating_group NUMBER(10) DEFAULT NULL,
    service_identifier NUMBER(10) DEFAULT NULL,
    auth_app_id NUMBER(10),
    auth_session_type NUMBER(10),
    pani VARCHAR2(100) DEFAULT NULL,
    mac VARCHAR2(17) DEFAULT NULL,
    app_provided_party VARCHAR2(100) DEFAULT NULL,
    is_final_allocation NUMBER(10),
    origin_host VARCHAR2(150) DEFAULT NULL
);

CREATE OR REPLACE TRIGGER ro_session_tr
before insert on ro_session FOR EACH ROW
BEGIN
  auto_id(:NEW.id);
END ro_session_tr;
/
BEGIN map2users('ro_session'); END;
/
CREATE INDEX ro_session_hash_idx  ON ro_session (hash_entry, hash_id);

INSERT INTO version (table_name, table_version) values ('ro_session','3');
