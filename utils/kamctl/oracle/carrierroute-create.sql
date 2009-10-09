INSERT INTO version (table_name, table_version) values ('carrierroute','3');
CREATE TABLE carrierroute (
    id NUMBER(10) PRIMARY KEY,
    carrier NUMBER(10) DEFAULT 0 NOT NULL,
    domain NUMBER(10) DEFAULT 0 NOT NULL,
    scan_prefix VARCHAR2(64) DEFAULT '',
    flags NUMBER(10) DEFAULT 0 NOT NULL,
    mask NUMBER(10) DEFAULT 0 NOT NULL,
    prob NUMBER DEFAULT 0 NOT NULL,
    strip NUMBER(10) DEFAULT 0 NOT NULL,
    rewrite_host VARCHAR2(128) DEFAULT '',
    rewrite_prefix VARCHAR2(64) DEFAULT '',
    rewrite_suffix VARCHAR2(64) DEFAULT '',
    description VARCHAR2(255) DEFAULT NULL
);

CREATE OR REPLACE TRIGGER carrierroute_tr
before insert on carrierroute FOR EACH ROW
BEGIN
  auto_id(:NEW.id);
END carrierroute_tr;
/
BEGIN map2users('carrierroute'); END;
/
INSERT INTO version (table_name, table_version) values ('carrierfailureroute','2');
CREATE TABLE carrierfailureroute (
    id NUMBER(10) PRIMARY KEY,
    carrier NUMBER(10) DEFAULT 0 NOT NULL,
    domain NUMBER(10) DEFAULT 0 NOT NULL,
    scan_prefix VARCHAR2(64) DEFAULT '',
    host_name VARCHAR2(128) DEFAULT '',
    reply_code VARCHAR2(3) DEFAULT '',
    flags NUMBER(10) DEFAULT 0 NOT NULL,
    mask NUMBER(10) DEFAULT 0 NOT NULL,
    next_domain NUMBER(10) DEFAULT 0 NOT NULL,
    description VARCHAR2(255) DEFAULT NULL
);

CREATE OR REPLACE TRIGGER carrierfailureroute_tr
before insert on carrierfailureroute FOR EACH ROW
BEGIN
  auto_id(:NEW.id);
END carrierfailureroute_tr;
/
BEGIN map2users('carrierfailureroute'); END;
/
INSERT INTO version (table_name, table_version) values ('carrier_name','1');
CREATE TABLE carrier_name (
    id NUMBER(10) PRIMARY KEY,
    carrier VARCHAR2(64) DEFAULT NULL
);

CREATE OR REPLACE TRIGGER carrier_name_tr
before insert on carrier_name FOR EACH ROW
BEGIN
  auto_id(:NEW.id);
END carrier_name_tr;
/
BEGIN map2users('carrier_name'); END;
/
INSERT INTO version (table_name, table_version) values ('domain_name','1');
CREATE TABLE domain_name (
    id NUMBER(10) PRIMARY KEY,
    domain VARCHAR2(64) DEFAULT NULL
);

CREATE OR REPLACE TRIGGER domain_name_tr
before insert on domain_name FOR EACH ROW
BEGIN
  auto_id(:NEW.id);
END domain_name_tr;
/
BEGIN map2users('domain_name'); END;
/
