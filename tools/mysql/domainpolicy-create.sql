INSERT INTO version (table_name, table_version) values ('domainpolicy','2');
CREATE TABLE domainpolicy (
    id INT(10) UNSIGNED AUTO_INCREMENT PRIMARY KEY NOT NULL,
    rule VARCHAR(255) NOT NULL,
    type VARCHAR(255) NOT NULL,
    att VARCHAR(255),
    val VARCHAR(128),
    comment VARCHAR(255) NOT NULL,
    UNIQUE KEY rav_idx (rule, att, val),
    KEY rule_idx (rule)
) ENGINE=MyISAM;

