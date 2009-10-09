INSERT INTO version (table_name, table_version) values ('domainpolicy','2');
CREATE TABLE domainpolicy (
    id NUMBER(10) PRIMARY KEY,
    rule VARCHAR2(255),
    type VARCHAR2(255),
    att VARCHAR2(255),
    val VARCHAR2(128),
    description VARCHAR2(255),
    CONSTRAINT domainpolicy_rav_idx  UNIQUE (rule, att, val)
);

CREATE OR REPLACE TRIGGER domainpolicy_tr
before insert on domainpolicy FOR EACH ROW
BEGIN
  auto_id(:NEW.id);
END domainpolicy_tr;
/
BEGIN map2users('domainpolicy'); END;
/
CREATE INDEX domainpolicy_rule_idx  ON domainpolicy (rule);

