INSERT INTO version (table_name, table_version) values ('dialplan','2');
CREATE TABLE dialplan (
    id NUMBER(10) PRIMARY KEY,
    dpid NUMBER(10),
    pr NUMBER(10),
    match_op NUMBER(10),
    match_exp VARCHAR2(64),
    match_len NUMBER(10),
    subst_exp VARCHAR2(64),
    repl_exp VARCHAR2(64),
    attrs VARCHAR2(64)
);

CREATE OR REPLACE TRIGGER dialplan_tr
before insert on dialplan FOR EACH ROW
BEGIN
  auto_id(:NEW.id);
END dialplan_tr;
/
BEGIN map2users('dialplan'); END;
/
