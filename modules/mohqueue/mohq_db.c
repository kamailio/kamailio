/*
 * Copyright (C) 2013 Robert Boisvert
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

#include "mohq.h"
#include "mohq_db.h"
#include "mohq_funcs.h"

/**********
* mohqueue definitions
**********/

str MOHQCSTR_ID = STR_STATIC_INIT ("id");
str MOHQCSTR_URI = STR_STATIC_INIT ("uri");
str MOHQCSTR_MDIR = STR_STATIC_INIT ("mohdir");
str MOHQCSTR_MFILE = STR_STATIC_INIT ("mohfile");
str MOHQCSTR_NAME = STR_STATIC_INIT ("name");
str MOHQCSTR_DEBUG = STR_STATIC_INIT ("debug");

static str *mohq_columns [] =
  {
  &MOHQCSTR_ID,
  &MOHQCSTR_URI,
  &MOHQCSTR_MDIR,
  &MOHQCSTR_MFILE,
  &MOHQCSTR_NAME,
  &MOHQCSTR_DEBUG,
  NULL
  };

/**********
* mohqcalls definitions
**********/

str CALLCSTR_CALL = STR_STATIC_INIT ("call_id");
str CALLCSTR_CNTCT = STR_STATIC_INIT ("call_contact");
str CALLCSTR_FROM = STR_STATIC_INIT ("call_from");
str CALLCSTR_MOHQ = STR_STATIC_INIT ("mohq_id");
str CALLCSTR_STATE = STR_STATIC_INIT ("call_status");
str CALLCSTR_TIME = STR_STATIC_INIT ("call_time");

static str *call_columns [] =
  {
  &CALLCSTR_STATE,
  &CALLCSTR_CALL,
  &CALLCSTR_MOHQ,
  &CALLCSTR_FROM,
  &CALLCSTR_CNTCT,
  &CALLCSTR_TIME,
  NULL
  };

/**********
* local function declarations
**********/

void set_call_key (db_key_t *, int, int);
void set_call_val (db_val_t *, int, int, void *);

/**********
* local functions
**********/

/**********
* Fill Call Keys
*
* INPUT:
*   Arg (1) = row pointer
*   Arg (2) = column count
* OUTPUT: none
**********/

void fill_call_keys (db_key_t *prkeys, int ncnt)

{
int nidx;
for (nidx = 0; nidx < ncnt; nidx++)
  { set_call_key (prkeys, nidx, nidx); }
return;
}

/**********
* Fill Call Values
*
* INPUT:
*   Arg (1) = row pointer
*   Arg (2) = call struct pointer
*   Arg (3) = column count
* OUTPUT: none
**********/

void fill_call_vals (db_val_t *prvals, call_lst *pcall, int ncnt)

{
int nstate = pcall->call_state / 100;
set_call_val (prvals, CALLCOL_STATE, CALLCOL_STATE, &nstate);
if (!ncnt)
  { return; }
set_call_val (prvals, CALLCOL_MOHQ, CALLCOL_MOHQ, &pcall->pmohq->mohq_id);
set_call_val (prvals, CALLCOL_CALL, CALLCOL_CALL, pcall->call_id);
set_call_val (prvals, CALLCOL_FROM, CALLCOL_FROM, pcall->call_from);
set_call_val (prvals, CALLCOL_CNTCT, CALLCOL_CNTCT, pcall->call_contact);
set_call_val (prvals, CALLCOL_TIME, CALLCOL_TIME, &pcall->call_time);
return;
}

/**********
* Set Call Column Key
*
* INPUT:
*   Arg (1) = row pointer
*   Arg (2) = column number
*   Arg (3) = column id
* OUTPUT: none
**********/

void set_call_key (db_key_t *prkeys, int ncol, int ncolid)

{
prkeys [ncol] = call_columns [ncolid];
return;
}

/**********
* Set Call Column Value
*
* INPUT:
*   Arg (1) = row pointer
*   Arg (2) = column number
*   Arg (3) = column id
*   Arg (4) = value pointer
* OUTPUT: none
**********/

void set_call_val (db_val_t *prvals, int ncol, int ncolid, void *pdata)

{
/**********
* fill based on column
**********/

switch (ncolid)
  {
  case CALLCOL_MOHQ:
  case CALLCOL_STATE:
    prvals [ncol].val.int_val = *((int *)pdata);
    prvals [ncol].type = DB1_INT;
    prvals [ncol].nul = 0;
    break;
  case CALLCOL_CALL:
  case CALLCOL_CNTCT:
  case CALLCOL_FROM:
    prvals [ncol].val.string_val = (char *)pdata;
    prvals [ncol].type = DB1_STRING;
    prvals [ncol].nul = 0;
    break;
  case CALLCOL_TIME:
    prvals [ncol].val.time_val = *((time_t *)pdata);
    prvals [ncol].type = DB1_DATETIME;
    prvals [ncol].nul = 0;
    break;
  }
return;
}

/**********
* external functions
**********/

/**********
* Add Call Record
*
* INPUT:
*   Arg (1) = call index
* OUTPUT: none
**********/

void add_call_rec (int ncall_idx)

{
/**********
* o fill column names and values
* o insert new record
**********/

char *pfncname = "add_call_rec: ";
db1_con_t *pconn = mohq_dbconnect ();
if (!pconn)
  { return; }
db_func_t *pdb = pmod_data->pdb;
pdb->use_table (pconn, &pmod_data->pcfg->db_ctable);
db_key_t prkeys [CALL_COLCNT];
fill_call_keys (prkeys, CALL_COLCNT);
db_val_t prvals [CALL_COLCNT];
call_lst *pcall = &pmod_data->pcall_lst [ncall_idx];
pcall->call_time = time (0);
fill_call_vals (prvals, pcall, CALL_COLCNT);
if (pdb->insert (pconn, prkeys, prvals, CALL_COLCNT) < 0)
  {
  LM_WARN ("%sUnable to add new row to %s", pfncname,
    pmod_data->pcfg->db_ctable.s);
  }
mohq_dbdisconnect (pconn);
return;
}

/**********
* Clear Call Records
*
* INPUT:
*   Arg (1) = connection pointer
* OUTPUT: none
**********/

void clear_calls (db1_con_t *pconn)

{
/**********
* delete all records
**********/

char *pfncname = "clear_calls: ";
db_func_t *pdb = pmod_data->pdb;
pdb->use_table (pconn, &pmod_data->pcfg->db_ctable);
if (pdb->delete (pconn, 0, 0, 0, 0) < 0)
  {
  LM_WARN ("%sUnable to delete all rows from %s", pfncname,
    pmod_data->pcfg->db_ctable.s);
  }
return;
}

/**********
* Delete Call Record
*
* INPUT:
*   Arg (1) = call pointer
* OUTPUT: none
**********/

void delete_call_rec (call_lst *pcall)

{
/**********
* o setup to delete based on call ID
* o delete record
**********/

char *pfncname = "delete_call_rec: ";
db1_con_t *pconn = mohq_dbconnect ();
if (!pconn)
  { return; }
db_func_t *pdb = pmod_data->pdb;
pdb->use_table (pconn, &pmod_data->pcfg->db_ctable);
db_key_t prkeys [1];
set_call_key (prkeys, 0, CALLCOL_CALL);
db_val_t prvals [1];
set_call_val (prvals, 0, CALLCOL_CALL, pcall->call_id);
if (pdb->delete (pconn, prkeys, 0, prvals, 1) < 0)
  {
  LM_WARN ("%sUnable to delete row from %s", pfncname,
    pmod_data->pcfg->db_ctable.s);
  }
mohq_dbdisconnect (pconn);
return;
}

/**********
* Connect to DB
*
* INPUT: none
* OUTPUT: DB connection pointer; NULL=failed
**********/

db1_con_t *mohq_dbconnect (void)

{
str *pdb_url = &pmod_data->pcfg->db_url;
db1_con_t *pconn = pmod_data->pdb->init (pdb_url);
if (!pconn)
  { LM_ERR ("Unable to connect to DB %s\n", pdb_url->s); }
return pconn;
}

/**********
* Disconnect from DB
*
* INPUT:
*   Arg (1) = connection pointer
* OUTPUT: none
**********/

void mohq_dbdisconnect (db1_con_t *pconn)

{
pmod_data->pdb->close (pconn);
return;
}

/**********
* Update Call Record
*
* INPUT:
*   Arg (1) = call pointer
* OUTPUT: none
**********/

void update_call_rec (call_lst *pcall)

{
/**********
* o setup to update based on call ID
* o update record
**********/

char *pfncname = "update_call_rec: ";
db1_con_t *pconn = mohq_dbconnect ();
if (!pconn)
  { return; }
db_func_t *pdb = pmod_data->pdb;
pdb->use_table (pconn, &pmod_data->pcfg->db_ctable);
db_key_t pqkeys [1];
set_call_key (pqkeys, 0, CALLCOL_CALL);
db_val_t pqvals [1];
set_call_val (pqvals, 0, CALLCOL_CALL, pcall->call_id);
db_key_t pukeys [1];
set_call_key (pukeys, 0, CALLCOL_STATE);
db_val_t puvals [1];
fill_call_vals (puvals, pcall, CALLCOL_STATE);
if (pdb->update (pconn, pqkeys, 0, pqvals, pukeys, puvals, 1, 1) < 0)
  {
  LM_WARN ("%sUnable to update row in %s", pfncname,
    pmod_data->pcfg->db_ctable.s);
  }
mohq_dbdisconnect (pconn);
return;
}

/**********
* Update Debug Record
*
* INPUT:
*   Arg (1) = MOH queue pointer
*   Arg (2) = debug flag
* OUTPUT: none
**********/

void update_debug (mohq_lst *pqueue, int bdebug)

{
/**********
* o setup to update based on queue name
* o update record
**********/

char *pfncname = "update_debug: ";
db1_con_t *pconn = mohq_dbconnect ();
if (!pconn)
  { return; }
db_func_t *pdb = pmod_data->pdb;
pdb->use_table (pconn, &pmod_data->pcfg->db_qtable);
db_key_t pqkeys [1] = { mohq_columns [MOHQCOL_NAME] };
db_val_t pqvals [1];
pqvals->val.string_val = pqueue->mohq_name;
pqvals->type = DB1_STRING;
pqvals->nul = 0;
db_key_t pukeys [1] = { mohq_columns [MOHQCOL_DEBUG] };
db_val_t puvals [1];
puvals->val.int_val = bdebug;
puvals->type = DB1_INT;
puvals->nul = 0;
if (pdb->update (pconn, pqkeys, 0, pqvals, pukeys, puvals, 1, 1) < 0)
  {
  LM_WARN ("%sUnable to update row in %s", pfncname,
    pmod_data->pcfg->db_qtable.s);
  }
mohq_dbdisconnect (pconn);
return;
}

/**********
* Update Message Queue List
*
* INPUT:
*   Arg (1) = connection pointer
* OUTPUT: none
**********/

void update_mohq_lst (db1_con_t *pconn)

{
/**********
* o reset checked flag on all queues
* o read queues from table
**********/

char *pfncname = "update_mohq_lst: ";
if (!pconn)
  { return; }
db_func_t *pdb = pmod_data->pdb;
mohq_lst *pqlst = pmod_data->pmohq_lst;
int nidx;
for (nidx = 0; nidx < pmod_data->mohq_cnt; nidx++)
  { pqlst [nidx].mohq_flags &= ~MOHQF_CHK; }
pdb->use_table (pconn, &pmod_data->pcfg->db_qtable);
db_key_t prkeys [MOHQ_COLCNT];
for (nidx = 0; nidx < MOHQ_COLCNT; nidx++)
  { prkeys [nidx] = mohq_columns [nidx]; }
db1_res_t *presult = NULL;
if (pdb->query (pconn, 0, 0, 0, prkeys, 0, MOHQ_COLCNT, 0, &presult))
  {
  LM_ERR ("%stable query (%s) failed!\n", pfncname,
    pmod_data->pcfg->db_qtable.s);
  return;
  }
db_row_t *prows = RES_ROWS (presult);
int nrows = RES_ROW_N (presult);
db_val_t *prowvals = NULL;
char *ptext, *puri;
mohq_lst *pnewlst;
for (nidx = 0; nidx < nrows; nidx++)
  {
  /**********
  * check URI
  **********/

  prowvals = ROW_VALUES (prows + nidx);
  char *pqname = (char *)VAL_STRING (prowvals + MOHQCOL_NAME);
  puri = (char *)VAL_STRING (prowvals + MOHQCOL_URI);
  struct sip_uri puri_parsed [1];
  if (parse_uri (puri, strlen (puri), puri_parsed))
    {
    LM_ERR ("Queue,Field (%s,%.*s): %s is not a valid URI!\n", pqname,
      STR_FMT (&MOHQCSTR_URI), puri);
    continue;
    }

  /**********
  * check MOHDIR
  **********/

  char *pmohdir;
  if (VAL_NULL (prowvals + MOHQCOL_MDIR))
    { pmohdir = pmod_data->pcfg->mohdir; }
  else
    {
    pmohdir = (char *)VAL_STRING (prowvals + MOHQCOL_MDIR);
    if (!*pmohdir)
      { pmohdir = pmod_data->pcfg->mohdir; }
    else
      {
      /**********
      * mohdir
      * o exists?
      * o directory?
      **********/

      struct stat psb [1];
      if (lstat (pmohdir, psb))
        {
        LM_ERR ("Queue,Field (%s,%.*s): Unable to find %s!\n", pqname,
          STR_FMT (&MOHQCSTR_MDIR), pmohdir);
        continue;
        }
      else
        {
        if ((psb->st_mode & S_IFMT) != S_IFDIR)
          {
          LM_ERR ("Queue,Field (%s,%.*s): %s is not a directory!\n", pqname,
            STR_FMT (&MOHQCSTR_MDIR), pmohdir);
          continue;
          }
        }
      }
    }

  /**********
  * check for MOH files
  **********/

  rtpmap **pmohfiles = find_MOH (pmohdir,
    (char *)VAL_STRING (prowvals + MOHQCOL_MFILE));
  if (!pmohfiles [0])
    {
    LM_ERR ("Queue,Field (%s,%.*s): Unable to find MOH files (%s/%s.*)!\n",
      pqname, STR_FMT (&MOHQCSTR_MDIR), pmohdir,
      (char *)VAL_STRING (prowvals + MOHQCOL_MFILE));
    continue;
    }

  /**********
  * find matching queues
  **********/

  int bfnd = 0;
  int nidx2;
  for (nidx2 = 0; nidx2 < pmod_data->mohq_cnt; nidx2++)
    {
    if (!strcasecmp (pqlst [nidx2].mohq_uri, puri))
      {
      /**********
      * o data the same?
      * o mark as found
      **********/

      if (strcmp (pqlst [nidx2].mohq_mohdir, pmohdir))
        {
        strcpy (pqlst [nidx2].mohq_mohdir, pmohdir);
        LM_INFO ("Queue,Field (%s,%.*s): Changed", pqname,
          STR_FMT (&MOHQCSTR_MDIR));
        }
      ptext = (char *)VAL_STRING (prowvals + MOHQCOL_MFILE);
      if (strcmp (pqlst [nidx2].mohq_mohfile, ptext))
        {
        strcpy (pqlst [nidx2].mohq_mohfile, ptext);
        LM_INFO ("Queue,Field (%s,%.*s): Changed", pqname,
          STR_FMT (&MOHQCSTR_MFILE));
        }
      ptext = (char *)VAL_STRING (prowvals + MOHQCOL_NAME);
      if (strcmp (pqlst [nidx2].mohq_name, ptext))
        {
        strcpy (pqlst [nidx2].mohq_name, ptext);
        LM_INFO ("Queue,Field (%s,%.*s): Changed", pqname,
          STR_FMT (&MOHQCSTR_NAME));
        }
      int bdebug = VAL_INT (prowvals + MOHQCOL_DEBUG) ? MOHQF_DBG : 0;
      if ((pqlst [nidx2].mohq_flags & MOHQF_DBG) != bdebug)
        {
        if (bdebug)
          { pqlst [nidx2].mohq_flags |= MOHQF_DBG; }
        else
          { pqlst [nidx2].mohq_flags &= ~MOHQF_DBG; }
        LM_INFO ("Queue,Field (%s,%.*s): Changed", pqname,
          STR_FMT (&MOHQCSTR_DEBUG));
        }
      bfnd = -1;
      pqlst [nidx2].mohq_flags |= MOHQF_CHK;
      break;
      }
    }

  /**********
  * add new queue
  **********/

  if (!bfnd)
    {
    /**********
    * o allocate new list
    * o copy old list
    * o add new row
    * o release old list
    * o adjust pointers to new list
    **********/

    int nsize = pmod_data->mohq_cnt + 1;
    pnewlst = (mohq_lst *) shm_malloc (sizeof (mohq_lst) * nsize);
    if (!pnewlst)
      {
      LM_ERR ("%sUnable to allocate shared memory!\n", pfncname);
      return;
      }
    pmod_data->mohq_cnt = nsize;
    if (--nsize)
      { memcpy (pnewlst, pqlst, sizeof (mohq_lst) * nsize); }
    pnewlst [nsize].mohq_id = prowvals [MOHQCOL_ID].val.int_val;
    pnewlst [nsize].mohq_flags = MOHQF_CHK;
    strcpy (pnewlst [nsize].mohq_uri, puri);
    strcpy (pnewlst [nsize].mohq_mohdir, pmohdir);
    strcpy (pnewlst [nsize].mohq_mohfile,
      (char *)VAL_STRING (prowvals + MOHQCOL_MFILE));
    strcpy (pnewlst [nsize].mohq_name,
      (char *)VAL_STRING (prowvals + MOHQCOL_NAME));
    if (VAL_INT (prowvals + MOHQCOL_DEBUG))
      { pnewlst [nsize].mohq_flags |= MOHQF_DBG; }
    LM_INFO ("Added new queue (%s)", pnewlst [nsize].mohq_name);
    if (nsize)
      { shm_free (pmod_data->pmohq_lst); }
    pmod_data->pmohq_lst = pnewlst;
    pqlst = pnewlst;
    }
  }

/**********
* find deleted queues
**********/

for (nidx = 0; nidx < pmod_data->mohq_cnt; nidx++)
  {
  /**********
  * o exists?
  * o if not last, replace current with last queue
  **********/

  if (pqlst [nidx].mohq_flags & MOHQF_CHK)
    { continue; }
  LM_INFO ("Removed queue (%s)", pqlst [nidx].mohq_name);
  if (nidx != (pmod_data->mohq_cnt - 1))
    {
    memcpy (&pqlst [nidx], &pqlst [pmod_data->mohq_cnt - 1],
      sizeof (mohq_lst));
    }
  --pmod_data->mohq_cnt;
  --nidx;
  }
return;
}
