CREATE TABLE rtpengine (
    id NUMBER(10) PRIMARY KEY,
    setid NUMBER(10) DEFAULT 0 NOT NULL,
    url VARCHAR2(64),
    weight NUMBER(10) DEFAULT 1 NOT NULL,
    disabled NUMBER(10) DEFAULT 0 NOT NULL,
    stamp DATE DEFAULT '1900-01-01 00:00:01',
    CONSTRAINT rtpengine_rtpengine_nodes  UNIQUE (setid, url)
);

CREATE OR REPLACE TRIGGER rtpengine_tr
before insert on rtpengine FOR EACH ROW
BEGIN
  auto_id(:NEW.id);
END rtpengine_tr;
/
BEGIN map2users('rtpengine'); END;
/
INSERT INTO version (table_name, table_version) values ('rtpengine','1');

