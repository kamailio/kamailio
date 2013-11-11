INSERT INTO version (table_name, table_version) values ('mohqcalls','1');
CREATE TABLE mohqcalls (
    id NUMBER(10) PRIMARY KEY,
    mohq_id NUMBER(10),
    call_id VARCHAR2(100),
    call_status NUMBER(10),
    call_from VARCHAR2(100),
    call_contact VARCHAR2(100),
    call_time DATE,
    CONSTRAINT mohqcalls_mohqcalls_idx  UNIQUE (call_id)
);

CREATE OR REPLACE TRIGGER mohqcalls_tr
before insert on mohqcalls FOR EACH ROW
BEGIN
  auto_id(:NEW.id);
END mohqcalls_tr;
/
BEGIN map2users('mohqcalls'); END;
/
INSERT INTO version (table_name, table_version) values ('mohqueues','1');
CREATE TABLE mohqueues (
    id NUMBER(10) PRIMARY KEY,
    name VARCHAR2(25),
    uri VARCHAR2(100),
    mohdir VARCHAR2(100),
    mohfile VARCHAR2(100),
    debug NUMBER(10),
    CONSTRAINT mohqueues_mohqueue_uri_idx  UNIQUE (uri),
    CONSTRAINT mohqueues_mohqueue_name_idx  UNIQUE (name)
);

CREATE OR REPLACE TRIGGER mohqueues_tr
before insert on mohqueues FOR EACH ROW
BEGIN
  auto_id(:NEW.id);
END mohqueues_tr;
/
BEGIN map2users('mohqueues'); END;
/
