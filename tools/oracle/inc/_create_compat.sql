create or replace function now(v1 in number := 0) return date is
Result date;
begin
 SELECT sysdate INTO Result FROM dual;
 return Result;
end now;
/

create or replace function rand(v1 in number := 0) return number is
Result number;
begin
 SELECT dbms_random.value INTO Result FROM dual;
 return Result;
end rand;
/

create or replace function concat(v1 in varchar2, v2 in varchar2, v3 in varchar2) return varchar2 IS
Result varchar2(4000);
begin
 SELECT v1||v2||v3 INTO Result from dual;
 return Result;
end concat;
/

create or replace TYPE TABLE_STRING IS TABLE OF VARCHAR2(4000);
/
create or replace function DUMP_TABLES(P_OWNER in VARCHAR2) RETURN TABLE_STRING
    PIPELINED
IS
  CURSOR COLUMNS_CUR (P_OWNER in VARCHAR2, P_TABLE in VARCHAR2) IS
    SELECT TABLE_NAME, COLUMN_NAME, DATA_TYPE
    FROM ALL_TAB_COLUMNS
    WHERE OWNER = UPPER(P_OWNER) AND TABLE_NAME = UPPER(P_TABLE)
    ORDER BY COLUMN_ID;

  COLUMN_REC COLUMNS_CUR%ROWTYPE;

  TABLE_REC_CUR SYS_REFCURSOR;
  L_QUERY VARCHAR2(8000);
  L_QUERY1 VARCHAR2(8000);
  L_QUERY2 VARCHAR2(8000);
  L_LINE VARCHAR2(8000);

  L_COMA CHAR(2) := '  ';

  FIRST_ROW BOOLEAN := TRUE;
BEGIN
FOR cur IN (SELECT TABLE_NAME FROM all_tables WHERE owner=UPPER(P_OWNER)) LOOP
  L_QUERY1 := 'SELECT ''INSERT INTO ' || cur.table_name;
  L_QUERY2 :='(';
  OPEN COLUMNS_CUR(P_OWNER, cur.table_name);
  FIRST_ROW := TRUE;
  LOOP
    FETCH COLUMNS_CUR INTO COLUMN_REC;

    IF FIRST_ROW AND COLUMNS_CUR%NOTFOUND THEN
      PIPE ROW('Table ''' || P_OWNER || '.' || cur.table_name || ''' not found');
    END IF;

    EXIT WHEN COLUMNS_CUR%NOTFOUND;

    IF FIRST_ROW THEN
      L_QUERY2 := L_QUERY2 || COLUMN_REC.COLUMN_NAME;
      L_QUERY := ' VALUES ('' || ';
    ELSE
      L_QUERY2 := L_QUERY2||','||COLUMN_REC.COLUMN_NAME;
      L_COMA := ', ';
      L_QUERY := L_QUERY || ' || '', '' || ';
    END IF;

    IF COLUMN_REC.DATA_TYPE = 'VARCHAR2' OR COLUMN_REC.DATA_TYPE = 'CHAR' 
	OR COLUMN_REC.DATA_TYPE = 'CLOB' THEN
      L_QUERY := L_QUERY || 'NVL2(' || COLUMN_REC.COLUMN_NAME || ', '''''''' ||
      REPLACE(' || COLUMN_REC.COLUMN_NAME || ', '''''''', '''''''''''') || '''''''', ''NULL'')';
    ELSIF COLUMN_REC.DATA_TYPE = 'DATE' THEN
      L_QUERY := L_QUERY || 'NVL2(' || COLUMN_REC.COLUMN_NAME || ', ''TO_DATE('''''' ||
      TO_CHAR(' || COLUMN_REC.COLUMN_NAME || ', ''yyyy-mm-dd hh24:mi:ss'') ||
      '''''', ''''yyyy-mm-dd hh24:mi:ss'''')'', ''NULL'')';
    ELSIF COLUMN_REC.DATA_TYPE = 'BLOB' THEN
      L_QUERY := L_QUERY || 'NVL2(' || COLUMN_REC.COLUMN_NAME || 
      ', ''UNSUPPORTED:NON EMPTY BLOB'', ''NULL'')';
    ELSE
      L_QUERY := L_QUERY || 'NVL(TO_CHAR(' || COLUMN_REC.COLUMN_NAME || '), ''NULL'')';
    END IF;

    FIRST_ROW := FALSE;

  END LOOP;

  IF NOT FIRST_ROW THEN
    L_QUERY :=L_QUERY1||L_QUERY2||')'|| L_QUERY || ' || '');'' AS LINE FROM ' || COLUMN_REC.TABLE_NAME;
  END IF;

  CLOSE COLUMNS_CUR;

 /* IF FIRST_ROW THEN
    RETURN;
  END IF;*/

  OPEN TABLE_REC_CUR FOR L_QUERY;

  LOOP
    FETCH TABLE_REC_CUR INTO L_LINE;
    EXIT WHEN TABLE_REC_CUR%NOTFOUND;
    PIPE ROW(L_LINE);
  END LOOP;
  CLOSE TABLE_REC_CUR;
END LOOP;
  RETURN;
END;
/
