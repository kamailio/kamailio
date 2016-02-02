CREATE TABLE rtpengine (
    setid NUMBER(10) DEFAULT 0 NOT NULL,
    url VARCHAR2(64),
    weight NUMBER(10) DEFAULT 1 NOT NULL,
    disabled NUMBER(10) DEFAULT 0 NOT NULL,
    CONSTRAINT rtpengine_rtpengine_nodes  PRIMARY KEY  (setid, url)
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

