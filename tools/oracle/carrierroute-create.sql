INSERT INTO VERSION (table_name,table_version) values ('carrierroute',2);
create table CARRIERROUTE
(
  ID             NUMBER(10) not null,
  CARRIER        NUMBER(10) default 0,
  DOMAIN         VARCHAR2(64) default '',
  SCAN_PREFIX    VARCHAR2(64) default '',
  FLAGS		 NUMBER(11) default 0,
  MASKS		 NUMBER(11) default 0,
  PROB           NUMBER default 0,
  STRIP          NUMBER(10) default 0,
  REWRITE_HOST   VARCHAR2(128) default '',
  REWRITE_PREFIX VARCHAR2(64) default '',
  REWRITE_SUFFIX VARCHAR2(64) default '',
  DESCRIPTION    VARCHAR2(255) default null
);
alter table CARRIERROUTE add constraint PK_CARRIERROUTE primary key (ID);
create or replace trigger carrierroute_tr
before insert on carrierroute FOR EACH ROW
BEGIN
  auto_id(:NEW.id);
END carrierroute_tr;
/
BEGIN map2users('CARRIERROUTE'); END;
/

INSERT INTO VERSION (table_name,table_version) values ('carrierfailueroute',1);
create table CARRIERFAILUREROUTE
(
  ID             NUMBER(10) not null,
  CARRIER        NUMBER(10) default 0,
  DOMAIN         VARCHAR2(64) default '',
  SCAN_PREFIX    VARCHAR2(64) default '',
  HOST_NAME      VARCHAR2(128) default '',
  REPLY_CODE     VARCHAR2(3) default '',
  FLAGS          NUMBER(11) default 0,
  MASK           NUMBER(11) default 0,
  NEXT_DOMAIN    VARCHAR2(64) default '',
  DESCRIPTION    VARCHAR2(255) default null
);
alter table CARRIERFAILUREROUTE add constraint PK_CARRIERFAILUREROUTE primary key (ID);
create or replace trigger carrierfailureroute_tr
before insert on carrierfailureroute FOR EACH ROW
BEGIN
  auto_id(:NEW.id);
END carrierfailureroute_tr;
/
BEGIN map2users('CARRIERFAILUREROUTE'); END;
/

INSERT INTO VERSION (table_name,table_version) values ('route_tree',1);
create table ROUTE_TREE
(
  ID      NUMBER(10) not null,
  CARRIER VARCHAR2(64) default null
);
alter table ROUTE_TREE add constraint PK_ROUTE_TREE primary key (ID);
create or replace trigger route_tree_tr
before insert on route_tree FOR EACH ROW
BEGIN
  auto_id(:NEW.id);
END route_tree_tr;
/
BEGIN map2users('ROUTE_TREE'); END;
/
