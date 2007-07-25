INSERT INTO version (table_name, table_version) values ('imc_rooms','1');
CREATE TABLE imc_rooms (
    id SERIAL PRIMARY KEY NOT NULL,
    name VARCHAR(64) NOT NULL,
    domain VARCHAR(64) NOT NULL,
    flag INTEGER NOT NULL,
    CONSTRAINT nd_imc UNIQUE (name, domain)
);

INSERT INTO version (table_name, table_version) values ('imc_members','1');
CREATE TABLE imc_members (
    id SERIAL PRIMARY KEY NOT NULL,
    username VARCHAR(64) NOT NULL,
    domain VARCHAR(64) NOT NULL,
    room VARCHAR(64) NOT NULL,
    flag INTEGER NOT NULL,
    CONSTRAINT ndr_imc UNIQUE (username, domain, room)
);

 