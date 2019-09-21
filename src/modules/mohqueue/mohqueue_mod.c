/*
 * Copyright (C) 2013-19 Robert Boisvert
 *
 * This file is part of the mohqueue module for Kamailio, a free SIP server.
 *
 * The mohqueue module is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version
 *
 * The mohqueue module is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA  02110-1301  USA
 *
 */

#include <sys/types.h>
#include <stdlib.h>

#include "mohqueue_mod.h"
#include "mohq_db.h"
#include "mohq_funcs.h"

MODULE_VERSION

/**********
* local function declarations
**********/

int fixup_count (void **, int);
static int mod_child_init (int);
static void mod_destroy (void);
static int mod_init (void);

/**********
* global varbs
**********/

mod_data *pmod_data;
pv_spec_t *prtp_pv;

/**********
* module exports
**********/

/* COMMANDS */
static cmd_export_t mod_cmds [] = {
  { "mohq_count", (cmd_function) mohq_count, 2, fixup_count, 0,
    REQUEST_ROUTE | FAILURE_ROUTE | ONREPLY_ROUTE },
  { "mohq_process", (cmd_function) mohq_process, 0, NULL, 0, REQUEST_ROUTE },
  { "mohq_retrieve", (cmd_function) mohq_retrieve, 2, fixup_spve_spve, 0,
    REQUEST_ROUTE | FAILURE_ROUTE | ONREPLY_ROUTE },
  { "mohq_send", (cmd_function) mohq_send, 1, fixup_spve_spve, 0, REQUEST_ROUTE },
  { NULL, NULL, -1, 0, 0 },
};

/* PARAMETERS */
str db_url = str_init (DEFAULT_DB_URL);
str db_ctable = str_init ("mohqcalls");
str db_qtable = str_init ("mohqueues");
char *mohdir = "";
int moh_maxcalls = 50;

static param_export_t mod_parms [] = {
  { "db_url", PARAM_STR, &db_url },
  { "db_ctable", PARAM_STR, &db_ctable },
  { "db_qtable", PARAM_STR, &db_qtable },
  { "mohdir", PARAM_STRING, &mohdir },
  { "moh_maxcalls", INT_PARAM, &moh_maxcalls },
  { NULL, 0, NULL },
};

static const char *mohqueue_rpc_debug_doc [] =
  {
  "Toggle mohqueue debug mode. Parameters: queue name, state (0=off, <>0=on)",
  0
  };

static const char *mohqueue_rpc_drop_call_doc [] =
  {
  "Drop a mohqueue call. Parameters: queue name, callID (*=all)",
  0
  };

/* RPC COMMANDS */
rpc_export_t mohqueue_rpc [] =
  {
    { "mohqueue.debug", mohqueue_rpc_debug, mohqueue_rpc_debug_doc, 0 },
    { "mohqueue.drop_call", mohqueue_rpc_drop_call,
      mohqueue_rpc_drop_call_doc, 0 },
    { 0, 0, 0, 0 }
  };

/* MODULE EXPORTS */
struct module_exports exports = {
  "mohqueue",       /* module name */
  DEFAULT_DLFLAGS,  /* dlopen flags */
  mod_cmds,         /* exported functions */
  mod_parms,        /* exported parameters */
  0,                /* RPC methods */
  0,                /* exported pseudo-variables */
  0,                /* response handling function */
  mod_init,         /* module initialization function */
  mod_child_init,   /* per-child initialization function */
  mod_destroy       /* destructor function */
};

/**********
* local constants
**********/

str prtpstat [1] = {STR_STATIC_INIT ("$rtpstat")};

/**********
* local functions
**********/

/**********
* Fixup Count
*
* INPUT:
*   Arg (1) = parameter array pointer
*   Arg (1) = parameter number
* OUTPUT: -1 if failed; 0 if saved as pv_elem_t
**********/

int fixup_count (void **param, int param_no)

{
if (param_no == 1)
  { return fixup_spve_spve (param, 1); }
if (param_no == 2)
  { return fixup_pvar_null (param, 1); }
return 0;
}

/**********
* Configuration Initialization
*
* INPUT:
*   pmod_data memory allocated
*   configuration values set
* OUTPUT: 0 if failed; else pmod_data has config values
**********/

static int init_cfg (void)

{
int bfnd = 0;
int berror = 0;
struct stat psb [1];

/**********
* db_url, db_ctable, db_qtable exist?
**********/

if (!db_url.s || db_url.len <= 0)
  {
  LM_ERR ("db_url parameter not set!\n");
  berror = 1;
  }
if (!db_ctable.s || db_ctable.len <= 0)
  {
  LM_ERR ("db_ctable parameter not set!\n");
  berror = 1;
  }
if (!db_qtable.s || db_qtable.len <= 0)
  {
  LM_ERR ("db_qtable parameter not set!\n");
  berror = 1;
  }

/**********
* mohdir
* o exists?
* o directory?
**********/

if (!*mohdir)
  {
  LM_ERR ("mohdir parameter not set!\n");
  berror = 1;
  }
else if (strlen (mohdir) > MOHDIRLEN)
  {
  LM_ERR ("mohdir too long!\n");
  berror = 1;
  }
else
  {
  if (!lstat (mohdir, psb))
    {
    if ((psb->st_mode & S_IFMT) == S_IFDIR)
      { bfnd = 1; }
    }
  if (!bfnd)
    {
    LM_ERR ("mohdir is not a directory!\n");
    berror = 1;
    }
  }

/**********
* o max calls valid?
* o alloc memory
* o save data
**********/

if (moh_maxcalls < 1 || moh_maxcalls > 5000)
  {
  LM_ERR ("moh_maxcalls not in range of 1-5000!\n");
  berror = 1;
  }
if (berror)
  { return 0; }
pmod_data->pcall_lst =
  (call_lst *) shm_malloc (sizeof (call_lst) * moh_maxcalls);
if (!pmod_data->pcall_lst)
  {
  LM_ERR ("Unable to allocate shared memory!\n");
  return 0;
  }
pmod_data->pcfg->db_url = db_url;
pmod_data->pcfg->db_ctable = db_ctable;
pmod_data->pcfg->db_qtable = db_qtable;
pmod_data->pcfg->mohdir = mohdir;
memset (pmod_data->pcall_lst, 0, sizeof (call_lst) * moh_maxcalls);
pmod_data->call_cnt = moh_maxcalls;
return -1;
}

/**********
* DB Initialization
*
* INPUT:
*   pmod_data memory allocated and cfg values set
* OUTPUT: 0 if failed; else pmod_data has db_api
**********/

static int init_db (void)

{
/**********
* o bind to DB
* o check capabilities
* o init DB
**********/

str *pdb_url = &pmod_data->pcfg->db_url;
if (db_bind_mod (pdb_url, pmod_data->pdb))
  {
  LM_ERR ("Unable to bind DB API using %s!\n", pdb_url->s);
  return 0;
  }
db_func_t *pdb = pmod_data->pdb;
if (!DB_CAPABILITY ((*pdb), DB_CAP_ALL))
  {
  LM_ERR ("Selected database %s lacks required capabilities!\n", pdb_url->s);
  return 0;
  }
db1_con_t *pconn = mohq_dbconnect ();
if (!pconn)
  { return 0; }

/**********
* o check schema
* o remove all call recs
* o load queue list
**********/

if (db_check_table_version (pdb, pconn,
  &pmod_data->pcfg->db_ctable, MOHQ_CTABLE_VERSION) < 0)
  {
  DB_TABLE_VERSION_ERROR (pmod_data->pcfg->db_ctable);
  goto dberror;
  }
if (db_check_table_version (pdb, pconn,
  &pmod_data->pcfg->db_qtable, MOHQ_QTABLE_VERSION) < 0)
  {
  DB_TABLE_VERSION_ERROR (pmod_data->pcfg->db_qtable);
  goto dberror;
  }
clear_calls (pconn);
update_mohq_lst (pconn);
pmod_data->mohq_update = time (0);
mohq_dbdisconnect (pconn);
return -1;

/**********
* close DB
**********/

dberror:
pdb->close (pconn);
pconn = 0;
return 0;
}

/**********
* Child Module Initialization
*
* INPUT:
*   Arg (1) = child type
* OUTPUT: -1 if db_api not ready; else 0
**********/

int mod_child_init (int rank)

{
/**********
* o make sure DB initialized
**********/

if (rank == PROC_INIT || rank == PROC_TCP_MAIN || rank == PROC_MAIN)
  { return 0; }
if (!pmod_data->pdb->init)
  {
  LM_CRIT ("DB API not loaded!\n");
  return -1;
  }
return 0;
}

/**********
* Module Teardown
*
* INPUT: none
* OUTPUT: none
**********/

void mod_destroy (void)

{
/**********
* o destroy MOH can call queue locks
* o deallocate shared mem
**********/

if (!pmod_data)
  { return; }
if (pmod_data->pmohq_lock->plock)
  { mohq_lock_destroy (pmod_data->pmohq_lock); }
if (pmod_data->pcall_lock->plock)
  { mohq_lock_destroy (pmod_data->pcall_lock); }
if (pmod_data->pmohq_lst)
  { shm_free (pmod_data->pmohq_lst); }
if (pmod_data->pcall_lst)
  { shm_free (pmod_data->pcall_lst); }
shm_free (pmod_data);
return;
}

/**********
* Module Initialization
*
* INPUT: none
* OUTPUT: -1 if failed; 0 if success
**********/

int mod_init (void)

{
int rtplen;

/**********
* o allocate shared mem and init
* o init configuration data
* o init DB
**********/

pmod_data = (mod_data *) shm_malloc (sizeof (mod_data));
if (!pmod_data)
  {
  LM_ERR ("Unable to allocate shared memory!\n");
  return -1;
  }
memset (pmod_data, 0, sizeof (mod_data));
if (!init_cfg ())
  { goto initerr; }
if (!init_db ())
  { goto initerr; }

/**********
* o bind to SL/TM/RR modules
* o bind to RTPPROXY functions
**********/

if (sl_load_api (pmod_data->psl))
  {
  LM_ERR ("Unable to load SL module!\n");
  goto initerr;
  }
if (load_tm_api (pmod_data->ptm))
  {
  LM_ERR ("Unable to load TM module!\n");
  goto initerr;
  }
if (load_rr_api (pmod_data->prr))
  {
  LM_ERR ("Unable to load RR module!\n");
  goto initerr;
  }
pmod_data->fn_rtp_answer = find_export ("rtpproxy_answer", 0, 0);
if (!pmod_data->fn_rtp_answer)
  {
  LM_ERR ("Unable to load rtpproxy_answer!\n");
  goto initerr;
  }
pmod_data->fn_rtp_offer = find_export ("rtpproxy_offer", 0, 0);
if (!pmod_data->fn_rtp_offer)
  {
  LM_ERR ("Unable to load rtpproxy_offer!\n");
  goto initerr;
  }
pmod_data->fn_rtp_stream_c = find_export ("rtpproxy_stream2uac", 2, 0);
if (!pmod_data->fn_rtp_stream_c)
  {
  LM_ERR ("Unable to load rtpproxy_stream2uac!\n");
  goto initerr;
  }
pmod_data->fn_rtp_stream_s = find_export ("rtpproxy_stream2uas", 2, 0);
if (!pmod_data->fn_rtp_stream_s)
  {
  LM_ERR ("Unable to load rtpproxy_stream2uas!\n");
  goto initerr;
  }
pmod_data->fn_rtp_stop_c = find_export ("rtpproxy_stop_stream2uac", 0, 0);
if (!pmod_data->fn_rtp_stop_c)
  {
  LM_ERR ("Unable to load rtpproxy_stop_stream2uac!\n");
  goto initerr;
  }
pmod_data->fn_rtp_stop_s = find_export ("rtpproxy_stop_stream2uas", 0, 0);
if (!pmod_data->fn_rtp_stop_s)
  {
  LM_ERR ("Unable to load rtpproxy_stop_stream2uas!\n");
  goto initerr;
  }
pmod_data->fn_rtp_destroy = find_export ("rtpproxy_destroy", 0, 0);
if (!pmod_data->fn_rtp_destroy)
  {
  LM_ERR ("Unable to load rtpproxy_destroy!\n");
  goto initerr;
  }

/**********
* register RPC
**********/

if (rpc_register_array (mohqueue_rpc))
  {
  LM_ERR ("Unable to register RPC commands!\n");
  return -1;
  }

/**********
* get RTPSTAT pv spec
**********/

rtplen = pv_locate_name (prtpstat);
if(rtplen != prtpstat->len)
  {
  LM_ERR ("Unable to find RTPSTAT pv!\n");
  goto initerr;
  }
prtp_pv = pv_cache_get (prtpstat);
if(!prtp_pv)
  {
  LM_ERR ("Unable to find pv spec for RTPSTAT!\n");
  goto initerr;
  }

/**********
* init MOH and call queue locks
**********/

if (!mohq_lock_init (pmod_data->pmohq_lock))
  { goto initerr; }
if (!mohq_lock_init (pmod_data->pcall_lock))
  { goto initerr; }
return 0;

/**********
* o release shared mem
* o exit with error
**********/

initerr:
if (pmod_data->mohq_cnt)
  { shm_free (pmod_data->pmohq_lst); }
if (pmod_data->pcall_lock->plock)
  { mohq_lock_destroy (pmod_data->pcall_lock); }
shm_free (pmod_data);
pmod_data = NULL;
return -1;
}
