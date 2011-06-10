/**
 * $Id$
 *
 * Copyright (C) 2010 Daniel-Constantin Mierla (asipto.com)
 *
 * This file is part of Kamailio, a free SIP server.
 *
 * This file is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version
 *
 * This file is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307  USA
 *
 */

#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <unistd.h>

#include "../../dprint.h"
#include "../../ut.h"
#include "../../pt.h"
#include "../../events.h"
#include "../../pvar.h"
#include "../../rpc.h"
#include "../../rpc_lookup.h"
#include "../../route_struct.h"
#include "../../mem/shm_mem.h"
		       
#include "debugger_act.h"
#include "debugger_api.h"

#define DBG_CMD_SIZE 256

#define DBG_STATE_INIT	0
#define DBG_STATE_WAIT	1
#define DBG_STATE_NEXT	2

static str _dbg_state_list[] = {
	str_init("unknown"),
	str_init("init"),
	str_init("wait"),
	str_init("next"),
	{0, 0}
};

str *dbg_get_state_name(int t)
{
	switch(t) {
		case DBG_STATE_INIT:
			return &_dbg_state_list[1];
		case DBG_STATE_WAIT:
			return &_dbg_state_list[2];
		case DBG_STATE_NEXT:
			return &_dbg_state_list[3];
	}
	return &_dbg_state_list[0];
}

#define DBG_CFGTRACE_ON	(1<<0)
#define DBG_ABKPOINT_ON	(1<<1)
#define DBG_LBKPOINT_ON	(1<<2)

static str _dbg_status_list[] = {
	str_init("cfgtrace-on"),
	str_init("cfgtrace-off"),
	str_init("abkpoint-on"),
	str_init("abkpoint-off"),
	str_init("lbkpoint-on"),
	str_init("lbkpoint-off"),
	{0, 0}
};

str *dbg_get_status_name(int t)
{
	if(t&DBG_CFGTRACE_ON)
		return &_dbg_status_list[0];
	if(t&DBG_ABKPOINT_ON)
		return &_dbg_status_list[2];
	if(t&DBG_LBKPOINT_ON)
		return &_dbg_status_list[4];

	return &_dbg_state_list[0];
}

#define DBG_CMD_NOP		0
#define DBG_CMD_ERR		1
#define DBG_CMD_READ	2
#define DBG_CMD_NEXT	3
#define DBG_CMD_MOVE	4
#define DBG_CMD_SHOW	5
#define DBG_CMD_PVEVAL	6
#define DBG_CMD_PVLOG	7


static str _dbg_cmd_list[] = {
	str_init("nop"),
	str_init("err"),
	str_init("read"),
	str_init("next"),
	str_init("move"),
	str_init("show"),
	str_init("pveval"),
	str_init("pvlog"),
	{0, 0}
};

str *dbg_get_cmd_name(int t)
{
	switch(t) {
		case DBG_CMD_NOP:
			return &_dbg_cmd_list[0];
		case DBG_CMD_ERR:
			return &_dbg_cmd_list[1];
		case DBG_CMD_READ:
			return &_dbg_cmd_list[2];
		case DBG_CMD_NEXT:
			return &_dbg_cmd_list[3];
		case DBG_CMD_MOVE:
			return &_dbg_cmd_list[4];
		case DBG_CMD_SHOW:
			return &_dbg_cmd_list[5];
		case DBG_CMD_PVEVAL:
			return &_dbg_cmd_list[6];
		case DBG_CMD_PVLOG:
			return &_dbg_cmd_list[7];
	}
	return &_dbg_state_list[0];
}

/**
 *
 */
int _dbg_cfgtrace = 0;

/**
 *
 */
int _dbg_breakpoint = 0;

/**
 *
 */
int _dbg_cfgtrace_level = L_ERR;

/**
 *
 */
int _dbg_cfgtrace_facility = DEFAULT_FACILITY;

/**
 *
 */
char *_dbg_cfgtrace_prefix = "*** cfgtrace:";

/**
 *
 */
int _dbg_step_usleep = 100000;

/**
 *
 */
int _dbg_step_loops = 200;

/**
 *
 */
typedef struct _dbg_cmd
{
	unsigned int pid;
	unsigned int cmd;
	char buf[DBG_CMD_SIZE];
} dbg_cmd_t;

/**
 *
 */
typedef struct _dbg_pid
{
	unsigned int pid;
	unsigned int set;
	unsigned int state;
	dbg_cmd_t in;
	dbg_cmd_t out;
} dbg_pid_t;

/**
 *
 */
static dbg_pid_t *_dbg_pid_list = NULL;

/**
 *
 */
static int _dbg_pid_no = 0;

/**
 *
 */
typedef struct _dbg_bp
{
	str cfile;
	int cline;
	int set;
	struct _dbg_bp *next;
} dbg_bp_t;

/**
 *
 */
static dbg_bp_t *_dbg_bp_list = NULL;

/**
 * callback executed for each cfg action
 */
int dbg_cfg_trace(void *data)
{
	struct action *a;
	struct sip_msg *msg;
	int loop;
	int olen;
	str pvn;
	pv_spec_t pvs;
    pv_value_t val;
	void **srevp;
	str *an;

	srevp = (void**)data;

	a = (struct action *)srevp[0];
	msg = (struct sip_msg *)srevp[1];

	if(a==NULL || msg==NULL || _dbg_pid_list==NULL)
		return 0;

	an = dbg_get_action_name(a);

	if(_dbg_pid_list[process_no].set&DBG_CFGTRACE_ON)
	{
		if(is_printable(_dbg_cfgtrace_level))
		{
			LOG_(_dbg_cfgtrace_facility, _dbg_cfgtrace_level,
					_dbg_cfgtrace_prefix,
					" c=[%s] l=%d a=%d n=%.*s\n",
					a->cfile, a->cline, a->type, an->len, an->s
				);
		}
	}
	if(!(_dbg_pid_list[process_no].set&DBG_ABKPOINT_ON))
	{
		/* no breakpoints to be considered */
		return 0;
	}

	if(_dbg_pid_list[process_no].state==DBG_STATE_INIT)
	{
		LOG(_dbg_cfgtrace_level,
					"breakpoint hit: p=[%u] c=[%s] l=%d a=%d n=%.*s\n",
					_dbg_pid_list[process_no].pid,
					a->cfile, a->cline, a->type, an->len, an->s
				);
		_dbg_pid_list[process_no].in.cmd = DBG_CMD_NOP;
		_dbg_pid_list[process_no].state = DBG_STATE_WAIT;
	}

	loop = 1;
	while(loop)
	{
		switch(_dbg_pid_list[process_no].in.cmd)
		{
			case DBG_CMD_NOP:
				sleep_us(_dbg_step_usleep); 
			break;
			case DBG_CMD_MOVE:
				loop = 0;
				_dbg_pid_list[process_no].state=DBG_STATE_INIT;
				_dbg_pid_list[process_no].in.cmd = DBG_CMD_NOP;
				_dbg_pid_list[process_no].in.pid = 0;
			break;
			case DBG_CMD_NEXT:
				loop = 0;
				if(_dbg_pid_list[process_no].state==DBG_STATE_WAIT)
					_dbg_pid_list[process_no].state=DBG_STATE_NEXT;
				_dbg_pid_list[process_no].in.cmd = DBG_CMD_NOP;
				olen = snprintf(_dbg_pid_list[process_no].out.buf,
						DBG_CMD_SIZE,
						"exec [%s:%d] a=%d n=%.*s",
						a->cfile, a->cline, a->type, an->len, an->s);
				if(olen<0)
				{
					_dbg_pid_list[process_no].out.cmd = DBG_CMD_ERR;
					break;
				}
				_dbg_pid_list[process_no].out.cmd = DBG_CMD_READ;
			break;
			case DBG_CMD_PVEVAL:
			case DBG_CMD_PVLOG:
				loop = _dbg_pid_list[process_no].in.cmd;
				_dbg_pid_list[process_no].in.cmd = DBG_CMD_NOP;
				pvn.s = _dbg_pid_list[process_no].in.buf;
				pvn.len = strlen(pvn.s);
				if(pvn.len<=0)
				{
					LM_ERR("no pv to eval\n");
					break;
				}
				LM_DBG("pv to eval: %s\n", pvn.s);
				if(pv_parse_spec(&pvn, &pvs)<0)
				{
					LM_ERR("unable to parse pv [%s]\n", pvn.s);
					break;
				}
				memset(&val, 0, sizeof(pv_value_t));
				if(pv_get_spec_value(msg, &pvs, &val) != 0)
				{
					LM_ERR("unable to get pv value for [%s]\n", pvn.s);
					break;
				}
				if(val.flags&PV_VAL_NULL)
				{
					if(loop==DBG_CMD_PVEVAL)
					{
						olen = snprintf(_dbg_pid_list[process_no].out.buf,
							DBG_CMD_SIZE,
							"%s : t=null",
							pvn.s);
						if(olen<0)
						{
							_dbg_pid_list[process_no].out.cmd = DBG_CMD_ERR;
							break;
						}
						_dbg_pid_list[process_no].out.cmd = DBG_CMD_READ;
					} else {
						LOG(_dbg_cfgtrace_level,
								"breakpoint eval: %s : t=null\n",
								pvn.s
							);
					}
					break;
				}
				if(val.flags&PV_TYPE_INT)
				{
					if(loop==DBG_CMD_PVEVAL)
					{
						olen = snprintf(_dbg_pid_list[process_no].out.buf,
							DBG_CMD_SIZE,
							"%s : t=int v=%d",
							pvn.s, val.ri);
						if(olen<0)
						{
							_dbg_pid_list[process_no].out.cmd = DBG_CMD_ERR;
							break;
						}
						_dbg_pid_list[process_no].out.cmd = DBG_CMD_READ;
					} else {
						LOG(_dbg_cfgtrace_level,
								"breakpoint eval: %s : t=int v=%d\n",
								pvn.s, val.ri
							);
					}
					break;
				}
		
				if(loop==DBG_CMD_PVEVAL)
				{
					olen = snprintf(_dbg_pid_list[process_no].out.buf,
						DBG_CMD_SIZE,
						"%s : t=str v=%.*s",
						pvn.s, val.rs.len, val.rs.s);
					if(olen<0)
					{
						_dbg_pid_list[process_no].out.cmd = DBG_CMD_ERR;
						break;
					}
					_dbg_pid_list[process_no].out.cmd = DBG_CMD_READ;
				} else {
					LOG(_dbg_cfgtrace_level,
							"breakpoint eval: %s : t=str v=%.*s\n",
							pvn.s, val.rs.len, val.rs.s
						);
				}
			break;
			case DBG_CMD_SHOW:
				_dbg_pid_list[process_no].in.cmd = DBG_CMD_NOP;
				_dbg_pid_list[process_no].out.cmd = DBG_CMD_NOP;
				olen = snprintf(_dbg_pid_list[process_no].out.buf,
						DBG_CMD_SIZE,
						"at bkp [%s:%d] a=%d n=%.*s",
						a->cfile, a->cline, a->type, an->len, an->s);
				if(olen<0)
				{
					_dbg_pid_list[process_no].out.cmd = DBG_CMD_ERR;
					break;
				}
				_dbg_pid_list[process_no].out.cmd = DBG_CMD_READ;
			break;
			default:
				/* unknown command?!? - exit loop */
				_dbg_pid_list[process_no].in.cmd = DBG_CMD_NOP;
				_dbg_pid_list[process_no].state=DBG_STATE_INIT;
				loop = 0;
		}
	}
	return 0;
}

/**
 *
 */
int dbg_init_bp_list(void)
{
	if(_dbg_bp_list!=NULL)
		return -1;
	_dbg_bp_list = (dbg_bp_t*)shm_malloc(sizeof(dbg_bp_t));
	if(_dbg_bp_list==NULL)
		return -1;
	memset(_dbg_bp_list, 0, sizeof(dbg_bp_t));
	if(_dbg_breakpoint==1)
		_dbg_bp_list->set |= DBG_ABKPOINT_ON;
	if(_dbg_cfgtrace==1)
		_dbg_bp_list->set |= DBG_CFGTRACE_ON;
	sr_event_register_cb(SREV_CFG_RUN_ACTION, dbg_cfg_trace);
	return 0;
}

/**
 *
 */
int dbg_add_breakpoint(struct action *a, int bpon)
{
	int len;
	dbg_bp_t *nbp = NULL;
	if(_dbg_bp_list==NULL)
		return -1;
	len = strlen(a->cfile);
	len += sizeof(dbg_bp_t) + 1;
	nbp = (dbg_bp_t*)shm_malloc(len);
	if(nbp==NULL)
		return -1;
	memset(nbp, 0, len);
	nbp->set |= (bpon)?DBG_ABKPOINT_ON:0;
	nbp->cline = a->cline;
	nbp->cfile.s = (char*)nbp + sizeof(dbg_bp_t);
	strcpy(nbp->cfile.s, a->cfile);
	nbp->cfile.len = strlen(nbp->cfile.s);
	nbp->next =	_dbg_bp_list->next;
	_dbg_bp_list->next = nbp;
	return 0;
}

/**
 *
 */
int dbg_init_pid_list(void)
{
	_dbg_pid_no = get_max_procs();

	if(_dbg_pid_no<=0)
		return -1;
	if(_dbg_pid_list!=NULL)
		return -1;
	_dbg_pid_list = (dbg_pid_t*)shm_malloc(_dbg_pid_no*sizeof(dbg_pid_t));
	if(_dbg_pid_list==NULL)
		return -1;
	memset(_dbg_pid_list, 0, _dbg_pid_no*sizeof(dbg_pid_t));
	return 0;
}

/**
 *
 */
int dbg_init_mypid(void)
{
	if(_dbg_pid_list==NULL)
		return -1;
	if(process_no>=_dbg_pid_no)
		return -1;
	_dbg_pid_list[process_no].pid = (unsigned int)my_pid();
	if(_dbg_breakpoint==1)
		_dbg_pid_list[process_no].set |= DBG_ABKPOINT_ON;
	if(_dbg_cfgtrace==1)
		_dbg_pid_list[process_no].set |= DBG_CFGTRACE_ON;
	return 0;
}

/**
 *
 */
int dbg_get_pid_index(unsigned int pid)
{
	int i;
	for(i=0; i<_dbg_pid_no; i++)
	{
		if(_dbg_pid_list[i].pid == pid)
			return i;
	}
	return -1;
}

/**
 *
 */
static const char* dbg_rpc_bp_doc[2] = {
	"Breakpoint command",
	0
};

/**
 *
 */
static void  dbg_rpc_bp(rpc_t* rpc, void* ctx)
{
	int i;
	int limit;
	int lpid;
	str cmd;
	str val;
	int loop;

	if(_dbg_pid_list==NULL)
	{
		rpc->fault(ctx, 500, "Not initialized");
		return;
	}
	if (rpc->scan(ctx, "S", &cmd) < 1)
	{
		rpc->fault(ctx, 500, "Config breakpoint command missing");
		return;
	}
	i = 0;
	limit = _dbg_pid_no;
	if (rpc->scan(ctx, "*d", &lpid) == 1)
	{
		i = dbg_get_pid_index((unsigned int)lpid);
		if(i<0)
		{
			rpc->fault(ctx, 500, "No such pid");
			return;
		}
		limit = i + 1;
	} else {
		lpid = -1;
	}
	if(cmd.len==2 && strncmp(cmd.s, "on", 2)==0)
	{
		for(; i<limit; i++)
		{
			_dbg_pid_list[i].set |=  DBG_ABKPOINT_ON;
			_dbg_pid_list[i].state=DBG_STATE_INIT;
		}
	} else if(cmd.len==3 && strncmp(cmd.s, "off", 3)==0) {
		for(; i<limit; i++)
		{
			_dbg_pid_list[i].set &=  ~DBG_ABKPOINT_ON;
			_dbg_pid_list[i].state=DBG_STATE_INIT;
		}
	} else if(cmd.len==7 && strncmp(cmd.s, "release", 7)==0) {
		for(; i<limit; i++)
		{
			if(_dbg_pid_list[i].state!=DBG_STATE_WAIT)
			{
				_dbg_pid_list[i].set &=  ~DBG_ABKPOINT_ON;
				_dbg_pid_list[i].state=DBG_STATE_INIT;
			}
		}
	} else if(cmd.len==4 && strncmp(cmd.s, "keep", 4)==0) {
		if(lpid==-1)
		{
			rpc->fault(ctx, 500, "Missing pid parameter");
			return;
		}
		for(loop=0; loop<_dbg_pid_no; loop++)
		{
			if(i!=loop)
			{
				_dbg_pid_list[loop].set &=  ~DBG_ABKPOINT_ON;
				if(_dbg_pid_list[loop].state!=DBG_STATE_INIT)
				{
					_dbg_pid_list[loop].in.pid = my_pid();
					_dbg_pid_list[loop].in.cmd = DBG_CMD_MOVE;
				}
			}
		}
	} else if(cmd.len==4 && strncmp(cmd.s, "move", 4)==0) {
		if(lpid==-1)
		{
			rpc->fault(ctx, 500, "Missing pid parameter");
			return;
		}
		for(; i<limit; i++)
		{
			if(_dbg_pid_list[i].state!=DBG_STATE_INIT)
			{
				_dbg_pid_list[i].set &=  ~DBG_ABKPOINT_ON;
				_dbg_pid_list[i].in.pid = my_pid();
				_dbg_pid_list[i].in.cmd = DBG_CMD_MOVE;
			}
		}
	} else if(cmd.len==4 && strncmp(cmd.s, "next", 4)==0) {
		if(lpid==-1)
		{
			rpc->fault(ctx, 500, "Missing pid parameter");
			return;
		}
		_dbg_pid_list[i].in.pid = my_pid();
		_dbg_pid_list[i].in.cmd = DBG_CMD_NEXT;
		for(loop=0; loop<_dbg_step_loops; loop++)
		{
			sleep_us(_dbg_step_usleep);
			if(_dbg_pid_list[i].out.cmd == DBG_CMD_READ)
			{
				rpc->add(ctx, "s", _dbg_pid_list[i].out.buf);
				_dbg_pid_list[i].out.cmd = DBG_CMD_NOP;
				return;
			} else if(_dbg_pid_list[i].out.cmd == DBG_CMD_ERR) {
				rpc->add(ctx, "s", "cmd execution error");
				_dbg_pid_list[i].out.cmd = DBG_CMD_NOP;
				return;
			}
		}
		/* nothing to read ... err?!? */
	} else if(cmd.len==4 && strncmp(cmd.s, "show", 4)==0) {
		if(lpid==-1)
		{
			rpc->fault(ctx, 500, "Missing pid parameter");
			return;
		}
		_dbg_pid_list[i].in.pid = my_pid();
		_dbg_pid_list[i].in.cmd = DBG_CMD_SHOW;
		for(loop=0; loop<_dbg_step_loops; loop++)
		{
			sleep_us(_dbg_step_usleep);
			if(_dbg_pid_list[i].out.cmd == DBG_CMD_READ)
			{
				rpc->add(ctx, "s", _dbg_pid_list[i].out.buf);
				_dbg_pid_list[i].out.cmd = DBG_CMD_NOP;
				return;
			} else if(_dbg_pid_list[i].out.cmd == DBG_CMD_ERR) {
				rpc->add(ctx, "s", "cmd execution error");
				_dbg_pid_list[i].out.cmd = DBG_CMD_NOP;
				return;
			}
		}
		/* nothing to read ... err?!? */
	} else if(cmd.len==4 && strncmp(cmd.s, "eval", 4)==0) {
		if(lpid==-1)
		{
			rpc->fault(ctx, 500, "Missing pid parameter");
			return;
		}
		if (rpc->scan(ctx, "S", &val) < 1)
		{
			rpc->fault(ctx, 500, "pv param missing");
			return;
		}
		if (val.len < 2 || val.len>=DBG_CMD_SIZE)
		{
			rpc->fault(ctx, 500, "invalid pv param");
			return;
		}
		strncpy(_dbg_pid_list[i].in.buf, val.s, val.len);
		_dbg_pid_list[i].in.buf[val.len] = '\0';
		_dbg_pid_list[i].in.pid = my_pid();
		_dbg_pid_list[i].in.cmd = DBG_CMD_PVEVAL;
		for(loop=0; loop<_dbg_step_loops; loop++)
		{
			sleep_us(_dbg_step_usleep);
			if(_dbg_pid_list[i].out.cmd == DBG_CMD_READ)
			{
				rpc->add(ctx, "s", _dbg_pid_list[i].out.buf);
				_dbg_pid_list[i].out.cmd = DBG_CMD_NOP;
				return;
			} else if(_dbg_pid_list[i].out.cmd == DBG_CMD_ERR) {
				rpc->add(ctx, "s", "cmd execution error");
				_dbg_pid_list[i].out.cmd = DBG_CMD_NOP;
				return;
			}
		}
		/* nothing to read ... err?!? */
	} else if(cmd.len==3 && strncmp(cmd.s, "log", 3)==0) {
		if(lpid==-1)
		{
			rpc->fault(ctx, 500, "Missing pid parameter");
			return;
		}
		if (rpc->scan(ctx, "S", &val) < 1)
		{
			rpc->fault(ctx, 500, "pv param missing");
			return;
		}
		if (val.len < 2 || val.len>=DBG_CMD_SIZE)
		{
			rpc->fault(ctx, 500, "invalid pv param");
			return;
		}
		strncpy(_dbg_pid_list[i].in.buf, val.s, val.len);
		_dbg_pid_list[i].in.buf[val.len] = '\0';
		_dbg_pid_list[i].in.pid = my_pid();
		_dbg_pid_list[i].in.cmd = DBG_CMD_PVLOG;
	} else {
		rpc->fault(ctx, 500, "Unknown inner command");
	}
	rpc->add(ctx, "s", "200 ok");
}

/**
 *
 */
static const char* dbg_rpc_list_doc[2] = {
	"List debugging process array",
	0
};

/**
 *
 */
static void  dbg_rpc_list(rpc_t* rpc, void* ctx)
{
	int i;
	int limit;
	int lpid;
	void* th;

	if(_dbg_pid_list==NULL)
	{
		rpc->fault(ctx, 500, "Not initialized");
		return;
	}
	i = 0;
	limit = _dbg_pid_no;
	if (rpc->scan(ctx, "*d", &lpid) == 1)
	{
		i = dbg_get_pid_index((unsigned int)lpid);
		if(i<0)
		{
			rpc->fault(ctx, 500, "No such pid");
			return;
		}
		limit = i + 1;
	}

	for(; i<limit; i++)
	{
		/* add entry node */
		if (rpc->add(ctx, "{", &th) < 0)
		{
			rpc->fault(ctx, 500, "Internal error creating rpc");
			return;
		}
		if(rpc->struct_add(th, "dddddd",
						"entry",  i,
						"pid",    _dbg_pid_list[i].pid,
						"set",    _dbg_pid_list[i].set,
						"state",  _dbg_pid_list[i].state,
						"in.pid", _dbg_pid_list[i].in.pid,
						"in.cmd", _dbg_pid_list[i].in.cmd
					)<0)
		{
			rpc->fault(ctx, 500, "Internal error creating rpc");
			return;
		}
	}
}

/**
 *
 */
static const char* dbg_rpc_trace_doc[2] = {
	"Config trace command",
	0
};

/**
 *
 */
static void  dbg_rpc_trace(rpc_t* rpc, void* ctx)
{
	int i;
	int limit;
	int lpid;
	str cmd;

	if(_dbg_pid_list==NULL)
	{
		rpc->fault(ctx, 500, "Not initialized");
		return;
	}
	if (rpc->scan(ctx, "S", &cmd) < 1)
	{
		rpc->fault(ctx, 500, "Config trace command missing");
		return;
	}
	i = 0;
	limit = _dbg_pid_no;
	if (rpc->scan(ctx, "*d", &lpid) == 1)
	{
		i = dbg_get_pid_index((unsigned int)lpid);
		if(i<0)
		{
			rpc->fault(ctx, 500, "No such pid");
			return;
		}
		limit = i + 1;
	}
	if(cmd.len!=2 && cmd.len!=3)
	{
		rpc->fault(ctx, 500, "Unknown trace command");
		return;
	}
	if(cmd.len==2)
	{
		if(strncmp(cmd.s, "on", 2)!=0)
		{
			rpc->fault(ctx, 500, "Unknown trace command");
			return;
		}
	} else {
		if(strncmp(cmd.s, "off", 3)!=0)
		{
			rpc->fault(ctx, 500, "Unknown trace command");
			return;
		}
	}
	for(; i<limit; i++)
	{
		if(cmd.len==2)
		{
			_dbg_pid_list[i].set |=  DBG_CFGTRACE_ON;
		} else {
			_dbg_pid_list[i].set &=  ~DBG_CFGTRACE_ON;
		}
	}
	rpc->add(ctx, "s", "200 ok");
}


/**
 *
 */
rpc_export_t dbg_rpc[] = {
	{"dbg.bp",    dbg_rpc_bp,        dbg_rpc_bp_doc,       0},
	{"dbg.ls",    dbg_rpc_list,      dbg_rpc_list_doc,     0},
	{"dbg.trace", dbg_rpc_trace,     dbg_rpc_trace_doc,    0},
	{0, 0, 0, 0}
};

/**
 *
 */
int dbg_init_rpc(void)
{
	if (rpc_register_array(dbg_rpc)!=0)
	{
		LM_ERR("failed to register RPC commands\n");
		return -1;
	}
	return 0;
}

