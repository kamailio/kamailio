INSERT INTO version (table_name, table_version) values ('dr_gateways','1');
CREATE TABLE dr_gateways (
    gwid INT(10) UNSIGNED AUTO_INCREMENT PRIMARY KEY NOT NULL,
    type INT(11) UNSIGNED DEFAULT 0 NOT NULL,
    address VARCHAR(128) NOT NULL,
    strip INT(11) UNSIGNED DEFAULT 0 NOT NULL,
    pri_prefix VARCHAR(64) DEFAULT NULL,
    description VARCHAR(128) DEFAULT '' NOT NULL
) ENGINE=MyISAM;

INSERT INTO version (table_name, table_version) values ('dr_rules','1');
CREATE TABLE dr_rules (
    ruleid INT(10) UNSIGNED AUTO_INCREMENT PRIMARY KEY NOT NULL,
    groupid VARCHAR(255) NOT NULL,
    prefix VARCHAR(64) NOT NULL,
    timerec VARCHAR(255) NOT NULL,
    priority INT(11) DEFAULT 0 NOT NULL,
    routeid VARCHAR(64) NOT NULL,
    gwlist VARCHAR(255) NOT NULL,
    description VARCHAR(128) DEFAULT '' NOT NULL
) ENGINE=MyISAM;

