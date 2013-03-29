INSERT INTO version (table_name, table_version) values ('rtpproxy','1');
CREATE TABLE rtpproxy (
    id NUMBER(10) PRIMARY KEY,
    setid VARCHAR2(32) DEFAULT 00 NOT NULL,
    url VARCHAR2(64) DEFAULT '',
    flags NUMBER(10) DEFAULT 0 NOT NULL,
    weight NUMBER(10) DEFAULT 1 NOT NULL,
    description VARCHAR2(64) DEFAULT ''
);

CREATE OR REPLACE TRIGGER rtpproxy_tr
before insert on rtpproxy FOR EACH ROW
BEGIN
  auto_id(:NEW.id);
END rtpproxy_tr;
/
BEGIN map2users('rtpproxy'); END;
/
