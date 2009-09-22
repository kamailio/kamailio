INSERT INTO version (table_name, table_version) values ('imc_rooms','1');
CREATE TABLE imc_rooms (
    id NUMBER(10) PRIMARY KEY,
    name VARCHAR2(64),
    domain VARCHAR2(64),
    flag NUMBER(10),
    CONSTRAINT imc_rooms_name_domain_idx  UNIQUE (name, domain)
);

CREATE OR REPLACE TRIGGER imc_rooms_tr
before insert on imc_rooms FOR EACH ROW
BEGIN
  auto_id(:NEW.id);
END imc_rooms_tr;
/
BEGIN map2users('imc_rooms'); END;
/
INSERT INTO version (table_name, table_version) values ('imc_members','1');
CREATE TABLE imc_members (
    id NUMBER(10) PRIMARY KEY,
    username VARCHAR2(64),
    domain VARCHAR2(64),
    room VARCHAR2(64),
    flag NUMBER(10),
    CONSTRAINT imc_members_account_room_idx  UNIQUE (username, domain, room)
);

CREATE OR REPLACE TRIGGER imc_members_tr
before insert on imc_members FOR EACH ROW
BEGIN
  auto_id(:NEW.id);
END imc_members_tr;
/
BEGIN map2users('imc_members'); END;
/
