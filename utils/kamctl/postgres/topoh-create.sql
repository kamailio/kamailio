CREATE TABLE topoh_address (
    id SERIAL PRIMARY KEY NOT NULL,
    trust INTEGER DEFAULT 1 NOT NULL,
    ip_addr VARCHAR(50) NOT NULL,
    mask INTEGER DEFAULT 32 NOT NULL,
    port SMALLINT DEFAULT 0 NOT NULL,
    tag VARCHAR(64)
);

INSERT INTO version (table_name, table_version) values ('topoh_address','1');

