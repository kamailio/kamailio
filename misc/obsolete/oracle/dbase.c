/*
 * Core functions.
 * See db/db.h for description.
 *
 * @todo add paranoid checks for sql statement buffer overflow, and protect
 *       the checks with macro which is switched off by default.
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

#include <stdlib.h>
#include <string.h>

#include "../../mem/mem.h"
#include "../../dprint.h"
#include "../../ut.h"

#include "common.h"
#include "dbase.h"
#include "utils.h"
#include "prepare.h"


static ora_param_t ora_params[ORA_PARAMS_MAX];
static sb2 ora_bind_inds[ORA_BINDS_MAX];


static int bind_values(db_con_t* _h, db_val_t* _v, int _n, int bound_count);
static int define_params(db_con_t* _h, int _nc);
static void free_params(void);
static int map_int_param_type_and_size_to_ext(db_con_t* _h, dvoid* p_param,
                                              int ora_param_num);
static int init_db_res(db_res_t** _r, int params_num);
static int convert_row(int params_num, db_res_t* _r);
static db_type_t sqlt_to_db_type(ub2 sqlt);

static int oci_prepare(db_con_t *_h);
static int oci_execute(db_con_t *_h, ub4 iters);
#if !DB_PREALLOC_SQLT_HANDLE
static int oci_cleanup(db_con_t *_h);
#endif

int db_use_table(db_con_t* _h, const char* _t)
{
  if (_h == NULL || _t == NULL)
  {
    ERR("%s: invalid (NULL) argument(s)\n", __FUNCTION__);
    return -1;
  }

  DBG("%s(%s)\n", __FUNCTION__, _t);
  
  CON_TABLE(_h) = _t; /* XXX: shouldn't we copy the string here instead? */
  return 0;
}

db_con_t* db_init(const char* _sqlurl)
{
  db_con_t  *p_db_con = NULL;
  ora_con_t *p_ora_con = NULL;
  sword      rc = OCI_ERROR;
  int        res = -1;

  char sz_user[32];
  char sz_passwd[32];
  char sz_db[32];

  LOG(L_ALERT, "WARNING! This module is experimental and may crash SER "
               "or create unexpected results. You use the module at your own "
               "risk. Please submit bugs at http://bugs.sip-router.org/\n");

  if (_sqlurl == NULL)
  {
    ERR("%s: invalid (NULL) argument\n", __FUNCTION__);
    goto db_init_err;
  }

  DBG("%s(%s)\n", __FUNCTION__, _sqlurl);


  if ((res = parse_sql_url(_sqlurl, sz_user, sz_passwd, sz_db)) < 0)
  {
    ERR("%s: Error %d while parsing %s\n", __FUNCTION__, res, _sqlurl);
    goto db_init_err;
  }


  p_db_con = pkg_malloc(sizeof(db_con_t));
  p_ora_con = pkg_malloc(sizeof(ora_con_t));

  if (p_db_con == NULL || p_ora_con == NULL)
  {
    ERR("%s: no memory\n", __FUNCTION__);
    goto db_init_err;
  }

  memset(p_db_con, 0, sizeof(db_con_t));
  memset(p_ora_con, 0, sizeof(ora_con_t));
  
  CON_TABLE(p_db_con) = NULL;
  /* CON_ORA() could not be used as lvalue due to deprecated lvalue casts. */
  CON_TAIL(p_db_con) = (unsigned long)p_ora_con;

  rc = OCIEnvCreate(&(p_ora_con->env.ptr), OCI_DEFAULT,
                    NULL, NULL, NULL, NULL, 0, NULL);
  if (rc != OCI_SUCCESS)
  {
    OCICHECK(p_ora_con->env.ptr, rc);
    goto db_init_err;
  }

  rc = OCIHandleAlloc(p_ora_con->env.ptr, &(p_ora_con->err.dvoid_ptr),
                      OCI_HTYPE_ERROR, 0, NULL);
  if (rc != OCI_SUCCESS)
  {
    OCICHECK(p_ora_con->err.ptr, rc);
    goto db_init_err;
  }

  rc = OCIHandleAlloc(p_ora_con->env.ptr, &(p_ora_con->svc.dvoid_ptr),
                      OCI_HTYPE_SVCCTX, 0, NULL);
  if (rc != OCI_SUCCESS)
  {
    OCICHECK(p_ora_con->env.ptr, rc);
    goto db_init_err;
  }

  rc = OCILogon(p_ora_con->env.ptr, p_ora_con->err.ptr, &(p_ora_con->svc.ptr),
                (OraText *)sz_user, strlen(sz_user),
                (OraText *)sz_passwd, strlen(sz_passwd),
                (OraText *)sz_db, strlen(sz_db));
  if (rc != OCI_SUCCESS)
  {
    OCICHECK(p_ora_con->err.ptr, rc);
    goto db_init_err;
  }

#if DB_PREALLOC_SQLT_HANDLE
  rc = OCIHandleAlloc(p_ora_con->env.ptr, &(p_ora_con->stmt.dvoid_ptr),
                      OCI_HTYPE_STMT, 0, NULL);
  if (rc != OCI_SUCCESS)
  {
    OCICHECK(p_ora_con->env.ptr, rc);
    goto db_init_err;
  }
#endif

  return p_db_con;

db_init_err:
#if DB_PREALLOC_SQLT_HANDLE
  if (p_ora_con->stmt.ptr != NULL)
    OCICHECK(p_ora_con->env.ptr, OCIHandleFree(p_ora_con->stmt.ptr, OCI_HTYPE_STMT));
#endif
  if (p_ora_con->svc.ptr != NULL)
    OCICHECK(p_ora_con->env.ptr, OCIHandleFree(p_ora_con->svc.ptr, OCI_HTYPE_SVCCTX));
  if (p_ora_con->err.ptr != NULL)
    OCICHECK(p_ora_con->env.ptr, OCIHandleFree(p_ora_con->err.ptr, OCI_HTYPE_ERROR));
  if (p_ora_con)
  {
    pkg_free(p_ora_con);
    p_ora_con = NULL;
  }
  if (p_db_con)
  {
    pkg_free(p_db_con);
    p_db_con = NULL;
  }

  ERR("%s failed\n", __FUNCTION__);
  return NULL;
}

void db_close(db_con_t* _h)
{
  sword rc = OCI_ERROR;

  if (_h == NULL)
  {
    ERR("%s: invalid (NULL) argument\n", __FUNCTION__);
    return;
  }
  
  DBG("%s\n", __FUNCTION__);

#if DB_PREALLOC_SQLT_HANDLE
  rc = OCIHandleFree(CON_ORA(_h)->stmt.ptr, OCI_HTYPE_STMT);
  if (rc != OCI_SUCCESS)
  {
    OCICHECK(CON_ORA(_h)->env.ptr, rc);
  }
#endif

  rc = OCILogoff(CON_ORA(_h)->svc.ptr, CON_ORA(_h)->err.ptr);
  if (rc != OCI_SUCCESS)
  {
    OCICHECK(CON_ORA(_h)->err.ptr, rc);
  }

  /* Note: svc.ptr handle was released by OCILogoff() function. */

  rc = OCIHandleFree(CON_ORA(_h)->err.ptr, OCI_HTYPE_ERROR);
  if (rc != OCI_SUCCESS)
  {
    OCICHECK(CON_ORA(_h)->env.ptr, rc);
  }

  rc = OCITerminate(OCI_DEFAULT);
  if (rc != OCI_SUCCESS)
  {
    OCICHECK(CON_ORA(_h)->env.ptr, rc);
  }

  if (CON_ORA(_h) != NULL)
  {
    pkg_free(CON_ORA(_h));
    CON_TAIL(_h) = (unsigned long)NULL;
  }
    
  if (_h != NULL)
  {
    pkg_free(_h);
    _h = NULL;
  }
}

int db_query(db_con_t* _h, db_key_t* _k, 
	         db_op_t* _op, db_val_t* _v, 
             db_key_t* _c, int _n, int _nc,
             db_key_t _o, db_res_t** _r)
{
  sword   rc = OCI_ERROR;
  int     params_num;

  *_r = NULL; /* Required for processing under err_cleanup label,
                 and simply to be on the safe side. */

  if (_h == NULL)
  {
    ERR("%s: invalid (NULL) argument(s)\n", __FUNCTION__);
    return -1;
  }

  if (ora_params[0].p_data != NULL)
  {
    ERR("%s: Nested calls to db_query() are not allowed!\n", __FUNCTION__);
    return -1;
  }

  DBG("%s\n", __FUNCTION__);

  prepare_select(_c, _nc);
  prepare_from(CON_TABLE(_h));
  prepare_where(_k, _op, _v, _n);
  prepare_order_by(_o);

  DBG("%.*s\n", prepared_sql_len(), prepared_sql());

  rc = oci_prepare(_h);
  if (rc != 0) return -1;

  rc = bind_values(_h, _v, _n, 0);
  if (rc < 0) goto err_cleanup;

  rc = oci_execute(_h, 0);
  if (rc != 0) goto err_cleanup;

  params_num = define_params(_h, _nc);
  if (params_num <= 0) { rc = -1; goto err_cleanup; }

  rc = init_db_res(_r, params_num);
  if (rc != 0) goto err_cleanup;

  while ((rc = OCIStmtFetch(CON_ORA(_h)->stmt.ptr, CON_ORA(_h)->err.ptr, 1, 0, 0))
            == OCI_SUCCESS)
  {
    rc = convert_row(params_num, *_r);
    if (rc != 0) goto err_cleanup;
  }

  if (rc != OCI_SUCCESS && rc != OCI_NO_DATA)
  {
    OCICHECK(CON_ORA(_h)->err.ptr, rc);
    rc = -1;
    goto err_cleanup;
  }
  else
  {
    rc = 0;
  }
  
  if (RES_ROW_N(*_r) == 0)
  {
    DBG("%s: no data\n", __FUNCTION__);
  }

err_cleanup:
#if !DB_PREALLOC_SQLT_HANDLE
  (void)oci_cleanup(_h);
#endif
  if (rc != 0)
  {
    (void)db_free_result(_h, *_r);
    return -1;
  }
  else
  {
    return 0;
  }
}

int db_raw_query(db_con_t* _h, char* _s, db_res_t** _r)
{
  ERR("%s: unimplemented\n", __FUNCTION__);
  *_r = NULL; /* Just in case */
  return -1;
}

int db_free_result(db_con_t* _h, db_res_t* _r)
{
  db_row_t *p_row;
  db_val_t *p_val;
  
  if (_h == NULL)
  {
    ERR("%s: invalid (NULL) argument(s)\n", __FUNCTION__);
    return -1;
  }

  DBG("%s\n", __FUNCTION__);

  free_params();
  
  if (_r == NULL)
    return 0;   /* Nothing else to free. */
    
  if (RES_ROW_N(_r) < 0)
  {
    ERR("%s: invalid RES_ROW_N(_r) value!\n", __FUNCTION__);
  }
    
  p_row = RES_ROWS(_r);
  while (RES_ROW_N(_r)-- > 0)
  {
    p_val = ROW_VALUES(p_row);

    while (ROW_N(p_row)--)
    {
      if (VAL_NULL(p_val) == 0)
      {
        if (VAL_TYPE(p_val) == DB_STRING && VAL_STRING(p_val) != NULL)
        {
          pkg_free((void *)VAL_STRING(p_val)); /* XXX Get rid of cast */
          VAL_STRING(p_val) = NULL;
          DBG("%s: DB_STRING memory released\n", __FUNCTION__);
        }
        else if (VAL_TYPE(p_val) == DB_BLOB && VAL_BLOB(p_val).s != NULL)
        {
          pkg_free(VAL_BLOB(p_val).s);
          VAL_BLOB(p_val).s = NULL;
          DBG("%s: DB_BLOB memory released\n", __FUNCTION__);
        }
        else if (VAL_TYPE(p_val) == DB_STR && VAL_STR(p_val).s != NULL)
        {
          pkg_free(VAL_STR(p_val).s);
          VAL_STR(p_val).s = NULL;
          DBG("%s: DB_STR memory released\n", __FUNCTION__);
        }
      }
      else
      {
        DBG("%s: NULL value skipped\n", __FUNCTION__);
      }
      
      ++p_val;
    }
    
    pkg_free(ROW_VALUES(p_row));
    ROW_VALUES(p_row) = NULL;
    DBG("%s: row memory released\n", __FUNCTION__);
    
    ++p_row;
  }

  if (RES_ROWS(_r) != NULL)
  {
    pkg_free(RES_ROWS(_r));
    RES_ROWS(_r) = NULL;
    DBG("%s: rows memory released\n", __FUNCTION__);
  }

  if (RES_TYPES(_r) != NULL)
  {
    pkg_free(RES_TYPES(_r));
    RES_TYPES(_r) = NULL;
    DBG("%s: types memory released\n", __FUNCTION__);
  }

  if (RES_NAMES(_r) != NULL)
  {
    pkg_free(RES_NAMES(_r));
    RES_NAMES(_r) = NULL;
    DBG("%s: names memory released\n", __FUNCTION__);
  }
    
  return 0;
}

int db_insert(db_con_t* _h, db_key_t* _k, db_val_t* _v, int _n)
{
  sword rc;
  
  if (_h == NULL || _k == NULL || _v == NULL || _n <= 0)
  {
    ERR("%s: invalid (NULL) argument(s)\n", __FUNCTION__);
    return -1;
  }

  DBG("%s\n", __FUNCTION__);

  prepare_insert(CON_TABLE(_h));
  prepare_insert_columns(_k, _n);
  prepare_insert_values(_v, _n);

  DBG("%.*s\n", prepared_sql_len(), prepared_sql());

  rc = oci_prepare(_h);
  if (rc != 0) goto err_cleanup;

  rc = bind_values(_h, _v, _n, 0);
  if (rc < 0) goto err_cleanup;

  rc = oci_execute(_h, 1);
  if (rc != 0) goto err_cleanup;

err_cleanup:
#if !DB_PREALLOC_SQLT_HANDLE
  (void)oci_cleanup(_h);
#endif
  return rc != 0 ? -1 : 0;
}

int db_delete(db_con_t* _h, db_key_t* _k, db_op_t* _op, db_val_t* _v, int _n)
{
  sword rc;
  
  if (_h == NULL)
  {
    ERR("%s: invalid (NULL) argument\n", __FUNCTION__);
    return -1;
  }

  DBG("%s\n", __FUNCTION__);

  prepare_delete(CON_TABLE(_h));
  prepare_where(_k, _op, _v, _n);

  DBG("%.*s\n", prepared_sql_len(), prepared_sql());

  rc = oci_prepare(_h);
  if (rc != 0) goto err_cleanup;

  rc = bind_values(_h, _v, _n, 0);
  if (rc < 0) goto err_cleanup;

  rc = oci_execute(_h, 1);
  if (rc != 0) goto err_cleanup;

err_cleanup:
#if !DB_PREALLOC_SQLT_HANDLE
  (void)oci_cleanup(_h);
#endif
  return rc != 0 ? -1 : 0;
}

int db_update(db_con_t* _h, db_key_t* _k, db_op_t* _op, db_val_t* _v,
              db_key_t* _uk, db_val_t* _uv, int _n, int _un)
{
  sword rc;
  
  if (_h == NULL || _uk == NULL || _uv == NULL || _un <= 0)
  {
    ERR("%s: invalid argument(s)\n", __FUNCTION__);
    return -1;
  }

  DBG("%s\n", __FUNCTION__);

  prepare_update(CON_TABLE(_h));
  prepare_update_set(_uk, _uv, _un);
  prepare_where(_k, _op, _v, _n);

  DBG("%.*s\n", prepared_sql_len(), prepared_sql());

  rc = oci_prepare(_h);
  if (rc != 0) goto err_cleanup;

  rc = bind_values(_h, _uv, _un, 0);
  if (rc < 0) goto err_cleanup;

  rc = bind_values(_h, _v, _n, rc);
  if (rc < 0) goto err_cleanup;

  rc = oci_execute(_h, 1);
  if (rc != 0) goto err_cleanup;

err_cleanup:
#if !DB_PREALLOC_SQLT_HANDLE
  (void)oci_cleanup(_h);
#endif
  return rc != 0 ? -1 : 0;
}

/* Return:  number of bound values (it could be different from _n parameter
 *          due to null values), or -1 in case of failure. */
static int bind_values(db_con_t* _h, db_val_t* _v, int _n, int bound_count)
{
  sword rc;
  int i;
  OCIBind   *p_bnd;
  dvoid     *p_bind_val;
  sb4        bind_val_sz;
  ub2        bind_val_type;
  /* XXX Will fail if more than single date is inserted!
   * It is necessary to allocate them dynamically! */
  static unsigned char ora_date[7] =
    { '\0', '\0', '\0', '\0', '\0', '\0', '\0' }; /* Oracle date format */

  for (i = 0; i < _n; ++i, ++_v)
  {
    if (VAL_NULL(_v))
    {
      DBG("%s: (null)\n", __FUNCTION__);
      /* 'null' is used in queries. So, nothing to bind.
         But we should keep correct index and 'for' condition. */
      --i;
      --_n;
      continue;
    }
    else
    {
      ora_bind_inds[i] = OCI_IND_NOTNULL;
    
      switch (VAL_TYPE(_v))
      {
        case DB_INT:
          p_bind_val = &VAL_INT(_v);
          bind_val_sz = sizeof(VAL_INT(_v));
          bind_val_type = SQLT_INT;
          DBG("%s: DB_INT - %d\n", __FUNCTION__, VAL_INT(_v));
          break;
  
        case DB_DOUBLE:
          p_bind_val = &VAL_DOUBLE(_v);
          bind_val_sz = sizeof(VAL_DOUBLE(_v));
          bind_val_type = SQLT_FLT;
          DBG("%s: DB_DOUBLE - %e\n", __FUNCTION__, VAL_DOUBLE(_v));
          break;
  
        case DB_STRING:
          p_bind_val = &VAL_STRING(_v);
          bind_val_sz = strlen(VAL_STRING(_v));
          /* XXX length should not exceed 4000 bytes here */
          bind_val_type = SQLT_CHR;
          DBG("%s: DB_STRING - %s\n", __FUNCTION__, VAL_STRING(_v));
          break;
  
        case DB_STR:
          p_bind_val = VAL_STR(_v).s;
          bind_val_sz = VAL_STR(_v).len;
          /* XXX length should not exceed 4000 bytes here */
          bind_val_type = SQLT_CHR;
          DBG("%s: DB_STR - %.*s\n", __FUNCTION__,
              VAL_STR(_v).len, ZSW(VAL_STR(_v).s) );
          break;
  
        case DB_DATETIME:
        {
          struct tm *p_tm = localtime(&VAL_TIME(_v));
          
          if (p_tm == NULL)
          {
            ERR("%s: time_t -> struct tm conversion failure %ld\n",
                __FUNCTION__, VAL_TIME(_v));
            return -1;
          }
          
          p_tm->tm_year += 1900; /* tm_year is stored as (Year - 1900) value. */
          ora_date[0] = p_tm->tm_year / 100 + 100;
          ora_date[1] = p_tm->tm_year % 100 + 100;
          ora_date[2] = p_tm->tm_mon + 1;  /* tm_mon is from [0-11] interval */
          ora_date[3] = p_tm->tm_mday;
          ora_date[4] = p_tm->tm_hour + 1;
          ora_date[5] = p_tm->tm_min + 1;
          ora_date[6] = p_tm->tm_sec + 1;

          /* XXX: Should tm_isdst, etc. be processed? */
          
          p_bind_val = ora_date;
          bind_val_sz = sizeof(ora_date);
          bind_val_type = SQLT_DAT;
          DBG("%s: DB_DATETIME - %s\n", __FUNCTION__,
              ctime(&VAL_TIME(_v)));
          break;
        }
  
        case DB_BLOB:
          p_bind_val = VAL_BLOB(_v).s;
          bind_val_sz = VAL_BLOB(_v).len;
          /* XXX length should not exceed 4000 bytes here (?) */
          bind_val_type = SQLT_VCS; /* XXX review */
          DBG("%s: DB_BLOB, length %d\n", __FUNCTION__, VAL_BLOB(_v).len);
          break;
  
        case DB_BITMAP:
          p_bind_val = &VAL_BITMAP(_v);
          bind_val_sz = sizeof(VAL_BITMAP(_v));
          bind_val_type = SQLT_UIN;
          DBG("%s: DB_BITMAP - %#x\n", __FUNCTION__, VAL_BITMAP(_v));
          break;
          
        default:
          ERR("%s: Unsupported DB value type - %d\n",
              __FUNCTION__, VAL_TYPE(_v));
          return -1;
      }
    }
  
    p_bnd = NULL; /* XXX Not used so far */
    rc = OCIBindByPos(CON_ORA(_h)->stmt.ptr, &p_bnd, CON_ORA(_h)->err.ptr,
                      bound_count + i + 1, p_bind_val, bind_val_sz,
                      bind_val_type, &ora_bind_inds[i], 0, 0, 0, 0,
                      OCI_DEFAULT);
    if (rc != OCI_SUCCESS)
    {
      OCICHECK(CON_ORA(_h)->err.ptr, rc);
      return -1;
    }
  }
  
  return _n;
}


/* Used by db_query().
 * free_params() must be called after the query is finished.
 * Return: number of parameters or -1 in case of failure;
 *         (free_params() must be called in both cases).
 * @sa free_params()
 */
static int define_params(db_con_t* _h, int _nc)
{
  OCIDefine *p_dfn = NULL;
  dvoid     *p_param = NULL;
  int        ora_params_num = 0;
  sword      rc;

#if DB_DEBUG
  int i = 0;
  for (; i < sizeof(ora_params) / sizeof(ora_params[0]); ++i)
  {
    if (ora_params[i].p_data != NULL)
    {
      ERR("%s: Internal logic is broken!\n", __FUNCTION__);
      return -1;
    }
  }
#endif

  /* XXX handle case when _nc is defined separately to avoid extra call
   *     to OCIParamGet() */
  while ((rc = OCIParamGet(CON_ORA(_h)->stmt.ptr, OCI_HTYPE_STMT,
                           CON_ORA(_h)->err.ptr,
                           &p_param, ora_params_num + 1)) == OCI_SUCCESS)
  {
    if (ora_params_num == ORA_PARAMS_MAX)
    {
      /* XXX log more info */
      ERR("%s: Too many table columns!\n", __FUNCTION__);
      return -1;
    }

    rc = map_int_param_type_and_size_to_ext(_h, p_param, ora_params_num);
    if (rc != 0) return -1;

    ora_params[ora_params_num].p_data =
      pkg_malloc(ora_params[ora_params_num].size);
  
    p_dfn = NULL; /* XXX currently unused */
    rc = OCIDefineByPos(CON_ORA(_h)->stmt.ptr, &p_dfn, CON_ORA(_h)->err.ptr,
                        ora_params_num + 1,
                        ora_params[ora_params_num].p_data,
                        ora_params[ora_params_num].size,
                        ora_params[ora_params_num].type,
                        &ora_params[ora_params_num].ind, 0, 0, OCI_DEFAULT);
    if (rc != OCI_SUCCESS)
    {
      OCICHECK(CON_ORA(_h)->err.ptr, rc);
      return -1;
    }
    
    ++ora_params_num;
  }

  if (rc != OCI_SUCCESS)
  {
    /*
     * OCI_ERROR + ORA-24334 are returned in case of invalid pos parameter of
     * OCIParamGet(). This is the only(?) way to handle 'select *'
     * statement while using non-scrollable cursor.
     */
    if (OCICHECK(CON_ORA(_h)->err.ptr, rc) != 24334 || rc != OCI_ERROR)
    {
      return -1;
    }
  }

  DBG("%s: ora_params_num = %d\n", __FUNCTION__, ora_params_num);

  return ora_params_num;
}

/* A companion for define_params() function
 * @sa define_params()
 */
static void free_params(void)
{
  int i = 0;
  int freed_count = 0;

  DBG("%s\n", __FUNCTION__);

  for (; i < sizeof(ora_params) / sizeof(ora_params[0]); ++i)
  {
    if (ora_params[i].p_data != NULL)
    {
      pkg_free(ora_params[i].p_data);
      ora_params[i].p_data = NULL;
      freed_count++;
    }
  }

  DBG("%s: %d parameter(s) freed\n", __FUNCTION__, freed_count);
}

/*
 * Called by define_params().
 * Uses and updates global ora_params[ora_params_num].
 * Defines the conversion rules that will be applied during fetching.
 * Don't forget to update sqlt_to_db_type() if output types are changed.
 */
static int map_int_param_type_and_size_to_ext(db_con_t* _h, dvoid* p_param,
                                              int ora_params_num)
{
  sword rc;
  
  rc = OCIAttrGet(p_param, OCI_DTYPE_PARAM, &ora_params[ora_params_num].type,
                  0, OCI_ATTR_DATA_TYPE, CON_ORA(_h)->err.ptr);
  if (rc != OCI_SUCCESS)
  {
    OCICHECK(CON_ORA(_h)->err.ptr, rc);
    return -1;
  }

  DBG1("%s: Oracle type %s\n", __FUNCTION__,
       sqlt_to_str(ora_params[ora_params_num].type));

  switch (ora_params[ora_params_num].type)
  {
    case SQLT_STR:
    case SQLT_VCS:
    case SQLT_CHR:
    {
      rc = OCIAttrGet(p_param, OCI_DTYPE_PARAM,
                      &ora_params[ora_params_num].size,
                      0, OCI_ATTR_DATA_SIZE, CON_ORA(_h)->err.ptr);
      if (rc != OCI_SUCCESS)
      {
        OCICHECK(CON_ORA(_h)->err.ptr, rc);
        return -1;
      }

      DBG1("%s: SQLT_CHR/VCS/STR size %d\n", __FUNCTION__,
           ora_params[ora_params_num].size);

      ora_params[ora_params_num].type = SQLT_STR;
      break;
    }
      
    case SQLT_INT:
      ora_params[ora_params_num].size = sizeof(int);
      break;

    case SQLT_UIN:
      ora_params[ora_params_num].size = sizeof(unsigned int);
      break;

    case SQLT_FLT:
      ora_params[ora_params_num].size = sizeof(double);
      break;

    case SQLT_DATE:
    case SQLT_TIMESTAMP:
    case SQLT_TIMESTAMP_TZ:
    case SQLT_TIMESTAMP_LTZ:
      /*
       * Let OCI make a conversion of date types
       * to the one that we understand.
       */
      ora_params[ora_params_num].type = SQLT_DAT;
      ora_params[ora_params_num].size = 7;
      break;

    case SQLT_DAT:
      ora_params[ora_params_num].size = 7;
      break;

    case SQLT_NUM:
    {
      sb2 precision;
      sb1 scale;
      
      rc = OCIAttrGet(p_param, OCI_DTYPE_PARAM,
                      &precision,
                      0, OCI_ATTR_PRECISION, CON_ORA(_h)->err.ptr);
      if (rc != OCI_SUCCESS)
      {
        OCICHECK(CON_ORA(_h)->err.ptr, rc);
        return -1;
      }

      rc = OCIAttrGet(p_param, OCI_DTYPE_PARAM,
                      &scale,
                      0, OCI_ATTR_SCALE, CON_ORA(_h)->err.ptr);
      if (rc != OCI_SUCCESS)
      {
        OCICHECK(CON_ORA(_h)->err.ptr, rc);
        return -1;
      }

      /*
       * Depending on precision and scale attributes we should decide
       * what external number type to use.
       */

      DBG1("%s: SQLT_NUM precision %d, scale %d\n", __FUNCTION__,
           precision, scale);

       
      if (precision < 9 && scale == 0)
      {
        /*
         * MAX_INT (= 134217727) occupies 9 digits. So, maximum integer
         * number supported is 99 999 999 (8 digits).
         */
        ora_params[ora_params_num].type = SQLT_INT;
        ora_params[ora_params_num].size = sizeof(int);
      }
      else
      {
        ora_params[ora_params_num].type = SQLT_FLT;
        ora_params[ora_params_num].size = sizeof(double);
      }
      break;
    }

    default:
      ERR("%s: Unsupported data type: %d!\n",
          __FUNCTION__, ora_params[ora_params_num].type);
      return -1;
  }

  DBG1("%s: External type %s, size %d\n", __FUNCTION__,
       sqlt_to_str(ora_params[ora_params_num].type),
       ora_params[ora_params_num].size);

  return 0;
}

/* @sa db_free_result() */
static int init_db_res(db_res_t** _r, int params_num)
{
  static db_res_t res;
  int i;
  
  *_r = &res;
  
  RES_NAMES(*_r) = (db_key_t *)pkg_malloc(sizeof(db_key_t) * params_num);
  RES_TYPES(*_r) = (db_type_t *)pkg_malloc(sizeof(db_type_t) * params_num);
  if (RES_NAMES(*_r) == NULL || RES_TYPES(*_r) == NULL)
  {
    ERR("%s: No memory for %d bytes!\n", __FUNCTION__,
        (sizeof(db_key_t) + sizeof(db_type_t)) * params_num);
    if ( RES_NAMES(*_r) != NULL )
    {
      pkg_free(RES_NAMES(*_r));
      RES_NAMES(*_r) = NULL;
    }
    if ( RES_TYPES(*_r) != NULL )
    {
      pkg_free(RES_TYPES(*_r));
      RES_TYPES(*_r) = NULL;
    }
    return -1;
  }

  RES_COL_N(*_r) = params_num;
  RES_ROWS(*_r)  = NULL;
  RES_ROW_N(*_r) = 0;

  for (i = 0; i < params_num; i++)
  {
    RES_NAMES(*_r)[i] = NULL; /* XXX unimplemented */
    RES_TYPES(*_r)[i] = sqlt_to_db_type(ora_params[i].type);
  }
  
  return 0;
}

/*
 * Only limited subset of SQLT_* types is allowed! The subset comes from
 * map_int_param_type_and_size_to_ext() function
 */
static db_type_t sqlt_to_db_type(ub2 sqlt)
{
  switch (sqlt)
  {
    case SQLT_STR: return DB_STR;
    case SQLT_INT: return DB_INT;
    case SQLT_UIN: return DB_BITMAP;
    case SQLT_FLT: return DB_DOUBLE;
    case SQLT_DAT: return DB_DATETIME;

    default:
      ERR("%s: Internal logic is broken!\n", __FUNCTION__);
      return -1;
  }
}

/* @sa db_free_result() */
static int convert_row(int params_num, db_res_t* _r)
{
  static int allocated_rows;

  int i;
  db_val_t *p_val;
  ora_param_t *p_param;

  DBG("%s: params_num = %d, _r = %p\n", __FUNCTION__, params_num, _r);

  /* Detect initial invocation. */
  if (RES_ROWS(_r) == NULL)
  {
    allocated_rows = 0;
  }

  /* 
   * Since we don't know number of rows aforetime, we aggressively
   * pre-allocate them to avoid multiple memory re-allocations.
   * The exact pre-allocation algorithm should be adjusted according
   * to actual queries. Let's use factor 10 for the beginning.
   */
  if (RES_ROW_N(_r) == allocated_rows)
  {
    void *tmp_ptr;
    
    allocated_rows *= 10;
    ++allocated_rows;   /* Handling initial 0 value. */

    /*
     * We can't assign directly to RES_ROWS(_r) here, since we don't want
     * to loose original pointer in case of realloc failure.
     */
    tmp_ptr = pkg_realloc(RES_ROWS(_r), sizeof(db_row_t) * allocated_rows);
    if (tmp_ptr == NULL)
    {
      ERR("%s: No memory for %d bytes!\n", __FUNCTION__,
          sizeof(db_row_t) * allocated_rows);
      return -1;
    }
    else
    {
      RES_ROWS(_r) = tmp_ptr;
    }
  }

  ROW_N(RES_ROWS(_r) + RES_ROW_N(_r)) = params_num;
  ROW_VALUES(RES_ROWS(_r) + RES_ROW_N(_r)) =
    p_val = pkg_malloc(sizeof(db_val_t) * params_num);
  ++RES_ROW_N(_r);
  
  p_param = ora_params;
  for (i = 0; i < params_num; i++, p_val++, p_param++)
  {
    VAL_TYPE(p_val) = sqlt_to_db_type(p_param->type);

    if (p_param->ind == OCI_IND_NULL)
    {
      VAL_NULL(p_val) = 1;

      DBG("OCI_IND_NULL, type %s\n", sqlt_to_str(p_param->type));
      continue;
    }
    else if (p_param->ind != OCI_IND_NOTNULL)
    {
      ERR("ind = %d, type %s\n", p_param->ind, sqlt_to_str(p_param->type));
    }

    VAL_NULL(p_val) = 0;
    
    switch (p_param->type)
    {
      case SQLT_STR:
      {
        /* We construct correct value of type DB_STR and at the same time
           of type DB_STRING (as long as _union_ is used inside db_val_t
           structure and string pointer is the _first_ variable of _str
           structure). This is necessary since the callers tend to ignore
           returned value types. Reported type is DB_STR. */
        
        size_t buflen = strlen(p_param->p_data) + 1;
        VAL_STR(p_val).s = pkg_malloc(buflen);
        if (VAL_STR(p_val).s == NULL)
        {
          /*
           * Set number of coulumns to actually processed value.
           * db_free_result() must honor this number during cleanup.
           */
          ROW_N(RES_ROWS(_r) + RES_ROW_N(_r) - 1) = i;
          return -1;
        }
        memcpy(VAL_STR(p_val).s, p_param->p_data, buflen);
        VAL_STR(p_val).len = buflen - 1;
        DBG("SQLT_STR: %.*s\n", VAL_STR(p_val).len, ZSW(VAL_STR(p_val).s));
        break;
      }

      case SQLT_INT:
        VAL_INT(p_val) = *(int *)(p_param->p_data);
        DBG("SQLT_INT: %d\n", *(int *)(p_param->p_data));
        break;

      case SQLT_UIN:
        VAL_BITMAP(p_val) = *(unsigned int *)(p_param->p_data);
        DBG("SQLT_UIN: %#x\n", *(unsigned int *)(p_param->p_data));
        break;

      case SQLT_FLT:
        VAL_DOUBLE(p_val) = *(double *)(p_param->p_data);
        DBG("SQLT_FLT: %e\n", *(double *)(p_param->p_data));
        break;
        
      case SQLT_DAT:
      {
        struct tm tm;
        time_t time;

        char *p_ora_date = p_param->p_data;

        tm.tm_year  = (*p_ora_date++ - 100) * 100;
        /*
         * Second step is necessary here, since single step operation
         * could result in wrong value due to double pointer++ operation.
         * 
         * 1900 is substracted since tm_year is stored as (Year - 1900) value.
         */
        tm.tm_year += *p_ora_date++ - 100 - 1900;
        tm.tm_mon   = *p_ora_date++ - 1; /* tm_mon is from [0-11] interval */
        tm.tm_mday  = *p_ora_date++;
        tm.tm_hour  = *p_ora_date++ - 1;
        tm.tm_min   = *p_ora_date++ - 1;
        tm.tm_sec   = *p_ora_date++ - 1;
        tm.tm_isdst = -1; /* XXX: unknown DST. Is it correct? */

        /* XXX Is it necessary to preset tm_gmtoff, tm_zone? */
        tm.tm_gmtoff = 0;
        tm.tm_zone  = 0;


        time = mktime(&tm);
        if (time == -1)
        {
          ERR("%s: SQLT_DAT -> time_t conversion failed\n", __FUNCTION__);
          return -1;
        }

        VAL_TIME(p_val) = time;
        DBG("SQLT_DAT: %s\n", ctime(&time));
        break;
      }
        
      default:
        /* All unsupported types must had been catched before. */
        ERR("%s: Internal logic is broken: %d!\n",
            __FUNCTION__, p_param->type);
        return -1;
    }
  }
  
  return 0;
}

/* Allocate and prepare SQL statement */
static int oci_prepare(db_con_t *_h)
{
  DBG("%s\n", __FUNCTION__);

#if !DB_PREALLOC_SQLT_HANDLE
  sword rc = OCIHandleAlloc(CON_ORA(_h)->env.ptr,
                            (dvoid **)&(CON_ORA(_h)->stmt.ptr),
                            OCI_HTYPE_STMT, 0, NULL);
  if (rc != OCI_SUCCESS)
  {
    OCICHECK(CON_ORA(_h)->env.ptr, rc);
    return -1;
  }
#else
  sword
#endif

  rc = OCIStmtPrepare(CON_ORA(_h)->stmt.ptr, CON_ORA(_h)->err.ptr,
                      (OraText *)prepared_sql(), prepared_sql_len(),
                      OCI_NTV_SYNTAX, OCI_DEFAULT);
  if (rc != OCI_SUCCESS)
  {
    OCICHECK(CON_ORA(_h)->err.ptr, rc);

#if !DB_PREALLOC_SQLT_HANDLE
    if (CON_ORA(_h)->stmt.ptr != NULL)
    {
      OCICHECK(CON_ORA(_h)->env.ptr,
               OCIHandleFree(CON_ORA(_h)->stmt.ptr, OCI_HTYPE_STMT));
    }
#endif
    return -2;
  }

  return 0;
}

/* 'iters' must be set to 0 for 'select' statement, and to 1 for others. */
static int oci_execute(db_con_t *_h, ub4 iters)
{
  DBG("%s\n", __FUNCTION__);

  sword rc = OCIStmtExecute(CON_ORA(_h)->svc.ptr, CON_ORA(_h)->stmt.ptr,
                            CON_ORA(_h)->err.ptr, iters, 0, NULL, NULL,
                            OCI_COMMIT_ON_SUCCESS);
  if (rc != OCI_SUCCESS)
  {
    OCICHECK(CON_ORA(_h)->err.ptr, rc);
    return -1;
  }
  return 0;
}

#if !DB_PREALLOC_SQLT_HANDLE
static int oci_cleanup(db_con_t *_h)
{
  DBG("%s\n", __FUNCTION__);

  sword rc = OCIHandleFree(CON_ORA(_h)->stmt.ptr, OCI_HTYPE_STMT);
  if (rc != OCI_SUCCESS)
  {
    OCICHECK(CON_ORA(_h)->env.ptr, rc);
    return -1;
  }
  return 0;
}
#endif
