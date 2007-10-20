INSERT INTO version (table_name, table_version) values ('imc_rooms','1');
CREATE TABLE imc_rooms (
    id SERIAL PRIMARY KEY NOT NULL,
    name VARCHAR(64) NOT NULL,
    domain VARCHAR(64) NOT NULL,
    flag INTEGER NOT NULL,
    CONSTRAINT name_domain_idx UNIQUE (name, domain)
);

INSERT INTO version (table_name, table_version) values ('imc_members','1');
CREATE TABLE imc_members (
    id SERIAL PRIMARY KEY NOT NULL,
    username VARCHAR(64) NOT NULL,
    domain VARCHAR(64) NOT NULL,
    room VARCHAR(64) NOT NULL,
    flag INTEGER NOT NULL,
    CONSTRAINT account_room_idx UNIQUE (username, domain, room)
);

