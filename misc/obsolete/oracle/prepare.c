/* 
 * It is assumed throughout this file that sqlbuf size is large enough.
 * Any paranoid checks should be performed outside.
 *
 * The code looks a bit ugly. This is for speed.
 * Please, don't add calls to sprintf(), etc.
 *
 * Copyright (C) 2005 RingCentral Inc.
 * Created by Dmitry Semyonov <dsemyonov@ringcentral.com>
 *
 *
 * This file is part of ser, a free SIP server.
 *
 * ser is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version
 *
 * For a license to use the ser software under conditions
 * other than those described here, or to purchase support for this
 * software, please contact iptel.org by e-mail at the following addresses:
 *    info@iptel.org
 *
 * ser is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License 
 * along with this program; if not, write to the Free Software 
 * Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA  02110-1301  USA
 */
 
#include <stdio.h>
#include <string.h>

#include "common.h"
#include "prepare.h"


#if SQL_STMT_BUFF_SZ < 1024
#warning It is not recommended to set SQL_STMT_BUFF_SZ to less than 1K
#endif

static char sqlbuf[SQL_STMT_BUFF_SZ];
static char *sqlbuf_pos = NULL;


static const char sql_eq_char = '=';

/*
 * ' is null ' or
 * ' is not null '
 * depending on _op.
 */
void prepare_null(db_op_t *_op)
{
  static const char     sql_str_eq[] = " is null";
  static const size_t   sql_str_eq_len = sizeof(sql_str_eq) - 1;

  static const char     sql_str_ne[] = " is not null";
  static const size_t   sql_str_ne_len = sizeof(sql_str_ne) - 1;
  
  if (_op == NULL || strcmp(*_op, OP_EQ) == 0)
  {
    memcpy(sqlbuf_pos, sql_str_eq, sql_str_eq_len);
    sqlbuf_pos += sql_str_eq_len;
  }
  else
  {
    memcpy(sqlbuf_pos, sql_str_ne, sql_str_ne_len);
    sqlbuf_pos += sql_str_ne_len;
  }
}

/*
 * where <name> <op> :a and <name> <op> :b and ... <name> <op> :n
 * or none.
 */
void prepare_where(db_key_t* _k, db_op_t* _op, db_val_t* _v, int _n)
{
  static const char     sql_str[] = "where ";
  static const size_t   sql_str_len = sizeof(sql_str) - 1;

  char sql_bind_char = 'a';
  
  /* XXX: assert(_n < 'z' - 'a' + 1) */
  
  if (_n > 0)
  {
    size_t cur_len;
    
    memcpy(sqlbuf_pos, sql_str, sql_str_len);
    sqlbuf_pos += sql_str_len;

    cur_len = strlen(*_k);
    memcpy(sqlbuf_pos, *_k++, cur_len);
    sqlbuf_pos += cur_len;

    if (VAL_NULL(_v++))
    {
      prepare_null(_op);
    }
    else
    {
      /* Default operation is '=' */
      if (_op == NULL)
      {
        *sqlbuf_pos++ = sql_eq_char;
      }
      else
      {
        cur_len = strlen(*_op);
        memcpy(sqlbuf_pos, *_op, cur_len);
        sqlbuf_pos += cur_len;
      }
    
      *sqlbuf_pos++ = ':';
      *sqlbuf_pos++ = sql_bind_char++;
    }

    if (_op != NULL)
      ++_op;
    
    while (--_n)
    {
      *sqlbuf_pos++ = ' ';
      *sqlbuf_pos++ = 'a';
      *sqlbuf_pos++ = 'n';
      *sqlbuf_pos++ = 'd';
      *sqlbuf_pos++ = ' ';
      
      cur_len = strlen(*_k);
      memcpy(sqlbuf_pos, *_k++, cur_len);
      sqlbuf_pos += cur_len;

      if (VAL_NULL(_v++))
      {
        prepare_null(_op);
      }
      else
      {
        /* Default operation is '=' */
        if (_op == NULL)
        {
          *sqlbuf_pos++ = sql_eq_char;
        }
        else
        {
          cur_len = strlen(*_op);
          memcpy(sqlbuf_pos, *_op, cur_len);
          sqlbuf_pos += cur_len;
        }
    
        *sqlbuf_pos++ = ':';
        *sqlbuf_pos++ = sql_bind_char++;
      }

      if (_op != NULL)
        ++_op;
    }

    *sqlbuf_pos++ = ' ';
  }
}


/*
 * ^select <col1>,<col2>,...,<colN>
 *  or
 * ^select *
 */
void prepare_select(db_key_t* _c, int _nc)
{
  static const char     sql_str[] = "select ";
  static const size_t   sql_str_len = sizeof(sql_str) - 1;
  
  memcpy(sqlbuf, sql_str, sql_str_len);
  sqlbuf_pos = sqlbuf + sql_str_len;

  if (_nc <= 0)
  {
    *sqlbuf_pos++ = '*';
  }
  else
  {
    size_t cur_len;
    
    cur_len = strlen(*_c);
    memcpy(sqlbuf_pos, *_c++, cur_len);
    sqlbuf_pos += cur_len;
    
    while (--_nc)
    {
      *sqlbuf_pos++ = ',';
      cur_len = strlen(*_c);
      memcpy(sqlbuf_pos, *_c++, cur_len);
      sqlbuf_pos += cur_len;
    }
  }

  *sqlbuf_pos++ = ' ';
}
  

/* from <table> */
void prepare_from(const char* _t)
{
  static const char     sql_str[] = "from ";
  static const size_t   sql_str_len = sizeof(sql_str) - 1;

  size_t cur_len;
  
  memcpy(sqlbuf_pos, sql_str, sql_str_len);
  sqlbuf_pos += sql_str_len;

  cur_len = strlen(_t);
  memcpy(sqlbuf_pos, _t, cur_len);
  sqlbuf_pos += cur_len;

  *sqlbuf_pos++ = ' ';
}


/*
 * order by <name>
 *  or none
 */
void prepare_order_by(db_key_t _o)
{
  if (_o != NULL)
  {
    static const char     sql_str[] = "order by ";
    static const size_t   sql_str_len = sizeof(sql_str) - 1;
  
    size_t cur_len;
    
    memcpy(sqlbuf_pos, sql_str, sql_str_len);
    sqlbuf_pos += sql_str_len;
  
    cur_len = strlen(_o);
    memcpy(sqlbuf_pos, _o, cur_len);
    sqlbuf_pos += cur_len;
  
    *sqlbuf_pos++ = ' ';
  }
}



/* ^insert into <table> */
void prepare_insert(const char* _t)
{
  static const char     sql_str[] = "insert into ";
  static const size_t   sql_str_len = sizeof(sql_str) - 1;

  size_t cur_len;
  
  memcpy(sqlbuf, sql_str, sql_str_len);
  sqlbuf_pos = sqlbuf + sql_str_len;

  cur_len = strlen(_t);
  memcpy(sqlbuf_pos, _t, cur_len);
  sqlbuf_pos += cur_len;

  *sqlbuf_pos++ = ' ';
}

/* (<col1>, <col2>, ..., <colN>) */
void prepare_insert_columns(db_key_t* _k, int _n)
{
  size_t cur_len;
  
  *sqlbuf_pos++ = '(';

  cur_len = strlen(*_k);
  memcpy(sqlbuf_pos, *_k++, cur_len);
  sqlbuf_pos += cur_len;
  
  while (--_n)
  {
    *sqlbuf_pos++ = ',';
    cur_len = strlen(*_k);
    memcpy(sqlbuf_pos, *_k++, cur_len);
    sqlbuf_pos += cur_len;
  }

  *sqlbuf_pos++ = ')';

  *sqlbuf_pos++ = ' ';
}

/* values (:a, :b, ..., :n) */
void prepare_insert_values(db_val_t* _v, int _n)
{
  static const char     sql_str[] = "values (";
  static const size_t   sql_str_len = sizeof(sql_str) - 1;

  char sql_bind_char = 'a';
  
  memcpy(sqlbuf_pos, sql_str, sql_str_len);
  sqlbuf_pos += sql_str_len;
  
  if (VAL_NULL(_v++))
  {
    *sqlbuf_pos++ = 'n';
    *sqlbuf_pos++ = 'u';
    *sqlbuf_pos++ = 'l';
    *sqlbuf_pos++ = 'l';
  }
  else
  {
    *sqlbuf_pos++ = ':';
    *sqlbuf_pos++ = sql_bind_char++;
  }
  
  while (--_n)
  {
    *sqlbuf_pos++ = ',';

    if (VAL_NULL(_v++))
    {
      *sqlbuf_pos++ = 'n';
      *sqlbuf_pos++ = 'u';
      *sqlbuf_pos++ = 'l';
      *sqlbuf_pos++ = 'l';
    }
    else
    {
      *sqlbuf_pos++ = ':';
      *sqlbuf_pos++ = sql_bind_char++;
    }
  }

  *sqlbuf_pos++ = ')';
  *sqlbuf_pos++ = ' ';
}




/* ^delete from <table> */
void prepare_delete(const char* _t)
{
  static const char     sql_str[] = "delete from ";
  static const size_t   sql_str_len = sizeof(sql_str) - 1;

  size_t cur_len;
  
  memcpy(sqlbuf, sql_str, sql_str_len);
  sqlbuf_pos = sqlbuf + sql_str_len;

  cur_len = strlen(_t);
  memcpy(sqlbuf_pos, _t, cur_len);
  sqlbuf_pos += cur_len;

  *sqlbuf_pos++ = ' ';
}




/* ^update <table> */
void prepare_update(const char* _t)
{
  static const char     sql_str[] = "update ";
  static const size_t   sql_str_len = sizeof(sql_str) - 1;

  size_t cur_len;
  
  memcpy(sqlbuf, sql_str, sql_str_len);
  sqlbuf_pos = sqlbuf + sql_str_len;

  cur_len = strlen(_t);
  memcpy(sqlbuf_pos, _t, cur_len);
  sqlbuf_pos += cur_len;

  *sqlbuf_pos++ = ' ';
}

/* set <name1>=:a,<name2>=:b,...,<nameN>=:n */
void prepare_update_set(db_key_t* _k, db_val_t* _v, int _n)
{
  size_t cur_len;
  char sql_bind_char = 'a';

  *sqlbuf_pos++ = 's';
  *sqlbuf_pos++ = 'e';
  *sqlbuf_pos++ = 't';
  *sqlbuf_pos++ = ' ';

  cur_len = strlen(*_k);
  memcpy(sqlbuf_pos, *_k++, cur_len);
  sqlbuf_pos += cur_len;

  *sqlbuf_pos++ = sql_eq_char;
  
  if (VAL_NULL(_v++))
  {
    *sqlbuf_pos++ = 'n';
    *sqlbuf_pos++ = 'u';
    *sqlbuf_pos++ = 'l';
    *sqlbuf_pos++ = 'l';
  }
  else
  {
    *sqlbuf_pos++ = ':';
    *sqlbuf_pos++ = sql_bind_char++;
  }
  
  while (--_n)
  {
    *sqlbuf_pos++ = ',';
    
    cur_len = strlen(*_k);
    memcpy(sqlbuf_pos, *_k++, cur_len);
    sqlbuf_pos += cur_len;

    *sqlbuf_pos++ = sql_eq_char;
  
    if (VAL_NULL(_v++))
    {
      *sqlbuf_pos++ = 'n';
      *sqlbuf_pos++ = 'u';
      *sqlbuf_pos++ = 'l';
      *sqlbuf_pos++ = 'l';
    }
    else
    {
      *sqlbuf_pos++ = ':';
      *sqlbuf_pos++ = sql_bind_char++;
    }
  }

  *sqlbuf_pos++ = ' ';
}


const char *prepared_sql(void)
{
  /* OCI statements should be null terminated according to OCI docs. */
  *sqlbuf_pos = '\0';
  return sqlbuf;
}

/* without trailing null character */
size_t prepared_sql_len(void)
{
  return sqlbuf_pos - sqlbuf;
}
