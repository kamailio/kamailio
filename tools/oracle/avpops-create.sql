INSERT INTO version (table_name, table_version) values ('usr_preferences','2');
CREATE TABLE usr_preferences (
    id NUMBER(10) PRIMARY KEY,
    uuid VARCHAR2(64) DEFAULT '',
    username VARCHAR2(128) DEFAULT 0 NOT NULL,
    domain VARCHAR2(64) DEFAULT '',
    attribute VARCHAR2(32) DEFAULT '',
    type NUMBER(10) DEFAULT 0 NOT NULL,
    value VARCHAR2(128) DEFAULT '',
    last_modified DATE DEFAULT to_date('1900-01-01 00:00:01','yyyy-mm-dd hh24:mi:ss')
);

CREATE OR REPLACE TRIGGER usr_preferences_tr
before insert on usr_preferences FOR EACH ROW
BEGIN
  auto_id(:NEW.id);
END usr_preferences_tr;
/
BEGIN map2users('usr_preferences'); END;
/
CREATE INDEX usr_preferences_ua_idx  ON usr_preferences (uuid, attribute);
CREATE INDEX usr_preferences_uda_idx  ON usr_preferences (username, domain, attribute);

