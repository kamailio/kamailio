CREATE TABLE security (
  id SERIAL PRIMARY KEY NOT NULL,
  action SMALLINT DEFAULT 0 NOT NULL,
  type SMALLINT DEFAULT 0 NOT NULL,
  data VARCHAR(64) DEFAULT '' NOT NULL
);

CREATE INDEX security_idx ON security (action, type, data);

INSERT INTO version (table_name, table_version) values ('security', '1');

