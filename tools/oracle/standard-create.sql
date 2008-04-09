create table VERSION
(
  TABLE_NAME    VARCHAR2(32) not null,
  TABLE_VERSION NUMBER(10) default 0
);
BEGIN map2users('VERSION'); END;
/
