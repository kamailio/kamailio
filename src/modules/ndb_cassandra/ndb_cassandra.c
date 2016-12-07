/*
 * ndb_cassandra.c
 *
 * Copyright (C) 2013 Indigital Telecom.
 *
 * Author: Luis Martin Gil
 *         <luis.martin.gil@indigital.net>
 *         <martingil.luis@gmail.com>
 *
 * This file is part of Kamailio, a free SIP server.
 *
 * kamailio is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version
 *
 * kamailio is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License 
 * along with this program; if not, write to the Free Software 
 * Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA  02110-1301  USA
 *
 * History:
 * -------
 * 2013-11: Initial version luismartingil
 */
/*! \file
 * \brief SIP-router indigital :: Module core
 * \ingroup cass
 */

/*! \defgroup INdigital Telecom  :: INdigital Telecom Cassandra writer.
 *
 */

#include "../../sr_module.h"
#include "../../mod_fix.h"
#include "../../lib/kmi/mi.h" //register_my_mod function
#include "../../lvalue.h"

#include <string.h>
#include <stdlib.h>

#include "thrift_wrapper.h" // Getting the thrift interface

MODULE_VERSION /* Module*/

/* Module parameter variables */
static str host        = {NULL, 0};
static int port        = 0;

/* Module management function prototypes */
static int mod_init(void);
static int child_init(int);
static void destroy(void);

/* Fixups functions */
static int fixup_cass_insert(void** param, int param_no);

static int fixup_cass_retrieve(void** param, int param_no);
static int free_fixup_cass_retrieve(void** param, int param_no);

/* Module exported functions */
static int cass_insert_f(struct sip_msg *msg, char* keyspace, char* column_family, 
			 char* key, char* column, char* value);
static int cass_retrieve_f(struct sip_msg *msg, char* keyspace, char* column_family, 
			   char* key, char* column, char* value);

/* Exported functions */
static cmd_export_t cmds[] = {
  {"cass_insert", (cmd_function)cass_insert_f, 5,
   fixup_cass_insert, 0,
   REQUEST_ROUTE|ONREPLY_ROUTE|FAILURE_ROUTE|BRANCH_ROUTE},
  {"cass_retrieve", (cmd_function)cass_retrieve_f, 5,
   fixup_cass_retrieve, free_fixup_cass_retrieve,
   REQUEST_ROUTE|ONREPLY_ROUTE|FAILURE_ROUTE|BRANCH_ROUTE},
  {0, 0, 0, 0, 0, 0}
};

/* Exported parameters */
static param_export_t params[] = {
  {"host",        PARAM_STR, &host},
  {"port",        INT_PARAM, &port},
  {0, 0, 0}
};

static mi_export_t mi_cmds[] = {
  { 0, 0, 0, 0, 0}
};

/* Module interface */
struct module_exports exports = {
    "ndb_cassandra",
    DEFAULT_DLFLAGS, /* dlopen flags */
    cmds,      /* Exported functions */
    params,    /* Exported parameters */
    0,         /* exported statistics */
    mi_cmds,   /* exported MI functions */
    0,         /* exported pseudo-variables */
    0,         /* extra processes */
    mod_init,  /* module initialization function */
    0,         /* response function*/
    destroy,   /* destroy function */
    child_init /* per-child init function */
};

/* Module initialization function */
static int mod_init(void) {
  if(register_mi_mod(exports.name, mi_cmds)!=0)
    {
      LM_ERR("failed to register MI commands\n");
      return -1;
    }    
  return 0;
}

/* Child initialization function */
static int child_init(int rank) {	
  int rtn = 0;
  return(rtn);
}

static void destroy(void) {
  return;
}
/**/
static int fixup_cass_insert(void** param, int param_no) {
  if (param_no == 1) {
    return fixup_spve_null(param, 1);
  }
  if (param_no == 2) {
    return fixup_spve_null(param, 1);
  }
  if (param_no == 3) {
    return fixup_spve_null(param, 1);
  }
  if (param_no == 4) {
    return fixup_spve_null(param, 1);
  }
  if (param_no == 5) {
    return fixup_spve_null(param, 1);
  }
  LM_ERR("invalid parameter number <%d>\n", param_no);
  return -1;
}
/**/
static int fixup_cass_retrieve(void** param, int param_no) {
  if (param_no == 1) {
    return fixup_spve_null(param, 1);
  }
  if (param_no == 2) {
    return fixup_spve_null(param, 1);
  }
  if (param_no == 3) {
    return fixup_spve_null(param, 1);
  }
  if (param_no == 4) {
    return fixup_spve_null(param, 1);
  }
  if (param_no == 5) {
    if (fixup_pvar_null(param, 1) != 0) {
      LM_ERR("failed to fixup result pvar\n");
      return -1;
    }
    if (((pv_spec_t *)(*param))->setf == NULL) {
      LM_ERR("result pvar is not writeble\n");
      return -1;
    }
    return 0;
  }
  
  LM_ERR("invalid parameter number <%d>\n", param_no);
  return -1;
}

static int free_fixup_cass_retrieve(void** param, int param_no) {
  if (param_no == 1) {
    LM_WARN("free function has not been defined for spve\n");
    return 0;
  }
  if (param_no == 2) {
    LM_WARN("free function has not been defined for spve\n");
    return 0;
  }
  if (param_no == 3) {
    LM_WARN("free function has not been defined for spve\n");
    return 0;
  }
  if (param_no == 4) {
    LM_WARN("free function has not been defined for spve\n");
    return 0;
  }
  if (param_no == 5) {
    return fixup_free_pvar_null(param, 1);
  }  
  LM_ERR("invalid parameter number <%d>\n", param_no);
  return -1;
}
/**/


// **********************************************************
// cass_insert_f
// **********************************************************
static int cass_insert_f(struct sip_msg *msg, char* keyspace, char* column_family, 
			 char* key, char* column, char* value) {
  
  str keyspace_string = {NULL, 0};
  if (fixup_get_svalue(msg, (gparam_p)keyspace, &keyspace_string) != 0) {
    LM_ERR("cannot get the keyspace value\n");goto error;}

  str column_family_string = {NULL, 0};
  if (fixup_get_svalue(msg, (gparam_p)column_family, &column_family_string) != 0) {
    LM_ERR("cannot get the column_family value\n");goto error;}

  str key_string = {NULL, 0};
  if (fixup_get_svalue(msg, (gparam_p)key, &key_string) != 0) {
    LM_ERR("cannot get the key value\n");goto error;}

  str column_string = {NULL, 0};
  if (fixup_get_svalue(msg, (gparam_p)column, &column_string) != 0) {
    LM_ERR("cannot get the column value\n");goto error;}

  str value_string = {NULL, 0};
  if (fixup_get_svalue(msg, (gparam_p)value, &value_string) != 0) {
    LM_ERR("cannot get the value value\n");goto error;}
  
  int ret;
  /* Doing the insert. */  
  LM_DBG("Insert.   %s['%s']['%s'] <== '%s' ",
	 column_family_string.s,
	 key_string.s,
	 column_string.s,
	 value_string.s);
  ret = insert_wrap(host.s, port,
		    keyspace_string.s,
		    column_family_string.s,
		    key_string.s,
		    column_string.s,
		    &(value_string.s));
  LM_DBG("Insert.   done!");
  return ret;
 error:
  return -1;
}
// cass_insert_f*********************************************

// **********************************************************
// cass_retrieve_f
// **********************************************************
static int cass_retrieve_f(struct sip_msg *msg, char* keyspace, char* column_family, 
			   char* key, char* column, char* value) {

  str keyspace_string = {NULL, 0};
  if (fixup_get_svalue(msg, (gparam_p)keyspace, &keyspace_string) != 0) {
    LM_ERR("cannot get the keyspace value\n");goto error;}

  str column_family_string = {NULL, 0};
  if (fixup_get_svalue(msg, (gparam_p)column_family, &column_family_string) != 0) {
    LM_ERR("cannot get the column_family value\n");goto error;}

  str key_string = {NULL, 0};
  if (fixup_get_svalue(msg, (gparam_p)key, &key_string) != 0) {
    LM_ERR("cannot get the key value\n");goto error;}

  str column_string = {NULL, 0};
  if (fixup_get_svalue(msg, (gparam_p)column, &column_string) != 0) {
    LM_ERR("cannot get the column value\n");goto error;}

  int ret;
  char *value_out = NULL;
  /* Doing the retrieve. */
  LM_DBG("Retrieve.   %s['%s']['%s'] ==>",
	 column_family_string.s,
	 key_string.s,
	 column_string.s);
  ret = retrieve_wrap(host.s, port,
		      keyspace_string.s,
		      column_family_string.s,
		      key_string.s,
		      column_string.s,
		      &(value_out));
  if (ret > 0) {
    LM_DBG("Retrieve.   done! value:'%s'", value_out);
    pv_spec_t* dst;
    pv_value_t val;
    val.rs.s = value_out;
    val.rs.len = strlen(value_out);
    val.flags = PV_VAL_STR;
    dst = (pv_spec_t *) value;
    dst->setf(msg, &dst->pvp, (int)EQ_T, &val);
    free(value_out);
  }
  return ret;
 error:
  return -1;
}
// cass_retrieve_f*******************************************
