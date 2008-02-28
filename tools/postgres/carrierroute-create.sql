INSERT INTO version (table_name, table_version) values ('carrierroute','1');
CREATE TABLE carrierroute (
    id SERIAL PRIMARY KEY NOT NULL,
    carrier INTEGER NOT NULL DEFAULT 0,
    scan_prefix VARCHAR(64) NOT NULL DEFAULT '',
    domain VARCHAR(64) NOT NULL DEFAULT '',
    prob REAL NOT NULL DEFAULT 0,
    strip INTEGER NOT NULL DEFAULT 0,
    rewrite_host VARCHAR(128) NOT NULL DEFAULT '',
    rewrite_prefix VARCHAR(64) NOT NULL DEFAULT '',
    rewrite_suffix VARCHAR(64) NOT NULL DEFAULT '',
    comment VARCHAR(255) DEFAULT NULL
);

INSERT INTO version (table_name, table_version) values ('carrierfailureroute','1');
CREATE TABLE carrierfailureroute (
    id SERIAL PRIMARY KEY NOT NULL,
    carrier INTEGER NOT NULL DEFAULT 0,
    domain VARCHAR(64) NOT NULL DEFAULT '',
    scan_prefix VARCHAR(64) NOT NULL DEFAULT '',
    host_name VARCHAR(128) NOT NULL DEFAULT '',
    reply_code VARCHAR(3) NOT NULL DEFAULT '',
    flags INTEGER NOT NULL DEFAULT 0,
    mask INTEGER NOT NULL DEFAULT 0,
    next_domain VARCHAR(64) NOT NULL DEFAULT '',
    comment VARCHAR(255) DEFAULT NULL
);

INSERT INTO version (table_name, table_version) values ('route_tree','1');
CREATE TABLE route_tree (
    id SERIAL PRIMARY KEY NOT NULL,
    carrier VARCHAR(64) DEFAULT NULL
);

