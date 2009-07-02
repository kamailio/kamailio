INSERT INTO version (table_name, table_version) values ('speed_dial','2');
CREATE TABLE speed_dial (
    id NUMBER(10) PRIMARY KEY,
    username VARCHAR2(64) DEFAULT '',
    domain VARCHAR2(64) DEFAULT '',
    sd_username VARCHAR2(64) DEFAULT '',
    sd_domain VARCHAR2(64) DEFAULT '',
    new_uri VARCHAR2(128) DEFAULT '',
    fname VARCHAR2(64) DEFAULT '',
    lname VARCHAR2(64) DEFAULT '',
    description VARCHAR2(64) DEFAULT '',
    CONSTRAINT speed_dial_speed_dial_idx  UNIQUE (username, domain, sd_domain, sd_username)
);

CREATE OR REPLACE TRIGGER speed_dial_tr
before insert on speed_dial FOR EACH ROW
BEGIN
  auto_id(:NEW.id);
END speed_dial_tr;
/
BEGIN map2users('speed_dial'); END;
/
