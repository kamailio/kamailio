postgresql (do not forget to add priveges for openser and openserro user to read fro mthis table)
========================
( do not forget to set the proper privileges for the domainpolicy table)

/*
 * domainpolicy table (see README domainpolicy module)
 */
CREATE TABLE domainpolicy (
 id             SERIAL PRIMARY KEY ,
 rule           VARCHAR(255) NOT NULL,
 type           VARCHAR(255) NOT NULL,
 att            VARCHAR(255),
 val            VARCHAR(255),
 comment        VARCHAR(255),
 UNIQUE ( rule, att, val )
);
CREATE INDEX domainpolicy_rule_idx ON domainpolicy(rule);




mysql
=======================
#
# domainpolicy table (see README domainpolicy module)
#
CREATE TABLE domainpolicy (
 id             INT NOT NULL PRIMARY KEY AUTO_INCREMENT,
 rule           VARCHAR(255) NOT NULL,
 type           VARCHAR(255) NOT NULL,
 att            VARCHAR(255),
 val            VARCHAR(255),
 comment        VARCHAR(255),
 UNIQUE         (rule, att, val),
 INDEX          rule_idx (rule)
);




postgresql and mysql
=====================

INSERT INTO version VALUES ( 'domainpolicy', '2');


