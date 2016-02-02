CREATE TABLE rtpengine (
    setid INTEGER DEFAULT 0 NOT NULL,
    url VARCHAR(64) NOT NULL,
    weight INTEGER DEFAULT 1 NOT NULL,
    disabled INTEGER DEFAULT 0 NOT NULL,
    CONSTRAINT rtpengine_rtpengine_nodes PRIMARY KEY  (setid, url)
);

INSERT INTO version (table_name, table_version) values ('rtpengine','1');

