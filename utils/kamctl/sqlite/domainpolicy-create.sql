INSERT INTO version (table_name, table_version) values ('domainpolicy','2');
CREATE TABLE domainpolicy (
    id INTEGER PRIMARY KEY NOT NULL,
    rule VARCHAR(255) NOT NULL,
    type VARCHAR(255) NOT NULL,
    att VARCHAR(255),
    val VARCHAR(128),
    description VARCHAR(255) NOT NULL,
    CONSTRAINT domainpolicy_rav_idx UNIQUE (rule, att, val)
);

CREATE INDEX domainpolicy_rule_idx ON domainpolicy (rule);

