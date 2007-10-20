INSERT INTO version (table_name, table_version) values ('imc_rooms','1');
CREATE TABLE imc_rooms (
    id INT(10) UNSIGNED AUTO_INCREMENT PRIMARY KEY NOT NULL,
    name VARCHAR(64) NOT NULL,
    domain VARCHAR(64) NOT NULL,
    flag INT(11) NOT NULL,
    UNIQUE KEY name_domain_idx (name, domain)
) ENGINE=MyISAM;

INSERT INTO version (table_name, table_version) values ('imc_members','1');
CREATE TABLE imc_members (
    id INT(10) UNSIGNED AUTO_INCREMENT PRIMARY KEY NOT NULL,
    username VARCHAR(64) NOT NULL,
    domain VARCHAR(64) NOT NULL,
    room VARCHAR(64) NOT NULL,
    flag INT(11) NOT NULL,
    UNIQUE KEY account_room_idx (username, domain, room)
) ENGINE=MyISAM;

