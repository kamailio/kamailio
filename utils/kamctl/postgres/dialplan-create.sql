INSERT INTO version (table_name, table_version) values ('dialplan','2');
CREATE TABLE dialplan (
    id SERIAL PRIMARY KEY NOT NULL,
    dpid INTEGER NOT NULL,
    pr INTEGER NOT NULL,
    match_op INTEGER NOT NULL,
    match_exp VARCHAR(64) NOT NULL,
    match_len INTEGER NOT NULL,
    subst_exp VARCHAR(64) NOT NULL,
    repl_exp VARCHAR(64) NOT NULL,
    attrs VARCHAR(64) NOT NULL
);

