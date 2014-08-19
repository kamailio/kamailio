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
 * Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA  02110-1301  USA
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
#include "../../locking.h"
#include "../../lvalue.h"
#include "../../hashes.h"
#include "../../lib/srutils/srjson.h"
#include "../../xavp.h"
#include "../pv/pv_xavp.h"

#include "debugger_act.h"
#include "debugger_api.h"
#include "debugger_config.h"

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
int _dbg_cfgpkgcheck = 0;

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
char *_dbg_cfgtrace_lname = NULL;

/**
 *
 */
int _dbg_step_usleep = 100000;

/**
 *
 */
int _dbg_step_loops = 200;

/**
 * disabled by default
 */
int _dbg_reset_msgid = 0;

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
	gen_lock_t *lock;
	unsigned int reset_msgid; /* flag to reset the id */
	unsigned int msgid_base; /* real id since the reset */
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

/* defined later */
int dbg_get_pid_index(unsigned int pid);

/*!
 * \brief Callback function that checks if reset_msgid is set
 *  and modifies msg->id if necessary.
 * \param msg SIP message
 * \param flags unused
 * \param bar unused
 * \return 1 on success, -1 on failure
 */
int dbg_msgid_filter(struct sip_msg *msg, unsigned int flags, void *bar)
{
	unsigned int process_no = my_pid();
	int indx = dbg_get_pid_index(process_no);
	unsigned int msgid_base = 0;
	unsigned int msgid_new = 0;
	if(indx<0) return -1;
	LM_DBG("process_no:%d indx:%d\n", process_no, indx);
	lock_get(_dbg_pid_list[indx].lock);
	if(_dbg_pid_list[indx].reset_msgid==1)
	{
		LM_DBG("reset_msgid! msgid_base:%d\n", msg->id);
		_dbg_pid_list[indx].reset_msgid = 0;
		_dbg_pid_list[indx].msgid_base = msg->id - 1;
	}
	msgid_base = _dbg_pid_list[indx].msgid_base;
	lock_release(_dbg_pid_list[indx].lock);
	if(msg->id > msgid_base)
	{
		msgid_new = msg->id - msgid_base;
		LM_DBG("msg->id:%d msgid_base:%d -> %d\n",
			msg->id, msgid_base, msgid_new);
		msg->id = msgid_new;
	}
	else
	{
		LM_DBG("msg->id:%d already processed\n", msg->id);
	}
	return 1;
}

char* get_current_route_type_name()
{
	switch(route_type){
		case REQUEST_ROUTE:
			return "request_route";
		case FAILURE_ROUTE:
			return "failure_route";
		case TM_ONREPLY_ROUTE:
		case CORE_ONREPLY_ROUTE:
		case ONREPLY_ROUTE:
			return "onreply_route";
		case BRANCH_ROUTE:
			return "branch_route";
		case ONSEND_ROUTE:
			return "onsend_route";
		case ERROR_ROUTE:
			return "error_route";
		case LOCAL_ROUTE:
			return "local_route";
		case BRANCH_FAILURE_ROUTE:
			return "branch_failure_route";
		default:
			return "unknown_route";
	}
}

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

	if(_dbg_cfgpkgcheck!=0)
	{
#if defined (PKG_MEMORY) && defined (q_malloc_h)
		LM_DBG("checking pkg memory before action %.*s (line %d)\n",
				an->len, an->s, a->cline);
		qm_check(mem_block);
#else
		LM_DBG("cfg pkg check is disbled due to missing qm handler\n");
#endif
	}

	if(_dbg_pid_list[process_no].set&DBG_CFGTRACE_ON)
	{
		if(is_printable(_dbg_cfgtrace_level))
		{
			LOG__(_dbg_cfgtrace_facility, _dbg_cfgtrace_level,
					_dbg_cfgtrace_lname, _dbg_cfgtrace_prefix,
					"%s=[%s] c=[%s] l=%d a=%d n=%.*s\n",
					get_current_route_type_name(), ZSW(a->rname),
					ZSW(a->cfile), a->cline,
					a->type, an->len, ZSW(an->s)
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
					ZSW(a->cfile), a->cline, a->type, an->len, ZSW(an->s)
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
						ZSW(a->cfile), a->cline, a->type, an->len, ZSW(an->s));
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
	if(_dbg_reset_msgid==1)
	{
		LM_DBG("[%d] create locks\n", process_no);
		_dbg_pid_list[process_no].lock = lock_alloc();
		if(_dbg_pid_list[process_no].lock==NULL)
		{
			LM_ERR("cannot allocate the lock\n");
			return -1;
		}
		if(lock_init(_dbg_pid_list[process_no].lock)==NULL)
		{
			LM_ERR("cannot init the lock\n");
			lock_dealloc(_dbg_pid_list[process_no].lock);
			return -1;
		}
	}
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
static const char* dbg_rpc_mod_level_doc[2] = {
	"Specify module log level",
	0
};

static void dbg_rpc_mod_level(rpc_t* rpc, void* ctx){
	int l;
	str value = {0,0};

	if (rpc->scan(ctx, "Sd", &value, &l) < 1)
	{
		rpc->fault(ctx, 500, "invalid parameters");
		return;
	}

	if(dbg_set_mod_debug_level(value.s, value.len, &l)<0)
	{
		rpc->fault(ctx, 500, "cannot store parameter\n");
		return;
	}
	rpc->add(ctx, "s", "200 ok");
}

/**
 *
 */
static const char* dbg_rpc_reset_msgid_doc[2] = {
	"Reset msgid on all process",
	0
};

static void dbg_rpc_reset_msgid(rpc_t* rpc, void* ctx){
	int i;
	if (_dbg_reset_msgid==0)
	{
		rpc->fault(ctx, 500, "reset_msgid is 0. Set it to 1 to enable.");
		return;
	}
	if(_dbg_pid_list==NULL)
	{
		rpc->fault(ctx, 500, "_dbg_pid_list is NULL");
		return;
	}
	LM_DBG("set reset_msgid\n");
	for(i=0; i<_dbg_pid_no; i++)
	{
		if (_dbg_pid_list[i].lock!=NULL)
		{
			lock_get(_dbg_pid_list[i].lock);
			_dbg_pid_list[i].reset_msgid = 1;
			lock_release(_dbg_pid_list[i].lock);
		}
	}
	rpc->add(ctx, "s", "200 ok");
}

/**
 *
 */
rpc_export_t dbg_rpc[] = {
	{"dbg.bp",        dbg_rpc_bp,        dbg_rpc_bp_doc,        0},
	{"dbg.ls",        dbg_rpc_list,      dbg_rpc_list_doc,      0},
	{"dbg.trace",     dbg_rpc_trace,     dbg_rpc_trace_doc,     0},
	{"dbg.mod_level", dbg_rpc_mod_level, dbg_rpc_mod_level_doc, 0},
	{"dbg.reset_msgid", dbg_rpc_reset_msgid, dbg_rpc_reset_msgid_doc, 0},
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

typedef struct _dbg_mod_level {
	str name;
	unsigned int hashid;
	int level;
	struct _dbg_mod_level *next;
} dbg_mod_level_t;

typedef struct _dbg_mod_slot
{
	dbg_mod_level_t *first;
	gen_lock_t lock;
} dbg_mod_slot_t;

static dbg_mod_slot_t *_dbg_mod_table = NULL;
static unsigned int _dbg_mod_table_size = 0;

/**
 *
 */
int dbg_init_mod_levels(int dbg_mod_hash_size)
{
	int i;
	if(dbg_mod_hash_size<=0)
		return 0;
	if(_dbg_mod_table!=NULL)
		return 0;
	_dbg_mod_table_size = 1 << dbg_mod_hash_size;
	_dbg_mod_table = (dbg_mod_slot_t*)shm_malloc(_dbg_mod_table_size*sizeof(dbg_mod_slot_t));
	if(_dbg_mod_table==NULL)
	{
		LM_ERR("no more shm.\n");
		return -1;
	}
	memset(_dbg_mod_table, 0, _dbg_mod_table_size*sizeof(dbg_mod_slot_t));

	for(i=0; i<_dbg_mod_table_size; i++)
	{
		if(lock_init(&_dbg_mod_table[i].lock)==0)
		{
			LM_ERR("cannot initalize lock[%d]\n", i);
			i--;
			while(i>=0)
			{
				lock_destroy(&_dbg_mod_table[i].lock);
				i--;
			}
			shm_free(_dbg_mod_table);
			_dbg_mod_table = NULL;
			return -1;
		}
	}
	return 0;
}

/*
 * case insensitive hashing - clone here to avoid usage of LOG*()
 * - s1 - str to hash
 * - s1len - len of s1
 * return computed hash id
 */
#define dbg_ch_h_inc h+=v^(v>>3)
#define dbg_ch_icase(_c) (((_c)>='A'&&(_c)<='Z')?((_c)|0x20):(_c))
static inline unsigned int dbg_compute_hash(char *s1, int s1len)
{
	char *p, *end;
	register unsigned v;
	register unsigned h;

	h=0;

	end=s1+s1len;
	for ( p=s1 ; p<=(end-4) ; p+=4 ){
		v=(dbg_ch_icase(*p)<<24)+(dbg_ch_icase(p[1])<<16)+(dbg_ch_icase(p[2])<<8)
			+ dbg_ch_icase(p[3]);
		dbg_ch_h_inc;
	}
	v=0;
	for (; p<end ; p++){ v<<=8; v+=dbg_ch_icase(*p);}
	dbg_ch_h_inc;

	h=((h)+(h>>11))+((h>>13)+(h>>23));
	return h;
}

int dbg_set_mod_debug_level(char *mname, int mnlen, int *mlevel)
{
	unsigned int idx;
	unsigned int hid;
	dbg_mod_level_t *it;
	dbg_mod_level_t *itp;
	dbg_mod_level_t *itn;

	if(_dbg_mod_table==NULL)
		return -1;

	hid = dbg_compute_hash(mname, mnlen);
	idx = hid&(_dbg_mod_table_size-1);

	lock_get(&_dbg_mod_table[idx].lock);
	it = _dbg_mod_table[idx].first;
	itp = NULL;
	while(it!=NULL && it->hashid < hid) {
		itp = it;
		it = it->next;
	}
	while(it!=NULL && it->hashid==hid)
	{
		if(mnlen==it->name.len
				&& strncmp(mname, it->name.s, mnlen)==0)
		{
			/* found */
			if(mlevel==NULL) {
				/* remove */
				if(itp!=NULL) {
					itp->next = it->next;
				} else {
					_dbg_mod_table[idx].first = it->next;
				}
				shm_free(it);
			} else {
				/* set */
				it->level = *mlevel;
			}
			lock_release(&_dbg_mod_table[idx].lock);
			return 0;
		}
		itp = it;
		it = it->next;
	}
	/* not found - add */
	if(mlevel==NULL) {
		lock_release(&_dbg_mod_table[idx].lock);
		return 0;
	}
	itn = (dbg_mod_level_t*)shm_malloc(sizeof(dbg_mod_level_t) + (mnlen+1)*sizeof(char));
	if(itn==NULL) {
		LM_ERR("no more shm\n");
		lock_release(&_dbg_mod_table[idx].lock);
		return -1;
	}
	memset(itn, 0, sizeof(dbg_mod_level_t) + (mnlen+1)*sizeof(char));
	itn->level    = *mlevel;
	itn->hashid   = hid;
	itn->name.s   = (char*)(itn) + sizeof(dbg_mod_level_t);
	itn->name.len = mnlen;
	strncpy(itn->name.s, mname, mnlen);
	itn->name.s[itn->name.len] = '\0';

	if(itp==NULL) {
		itn->next = _dbg_mod_table[idx].first;
		_dbg_mod_table[idx].first = itn;
	} else {
		itn->next = itp->next;
		itp->next = itn;
	}
	lock_release(&_dbg_mod_table[idx].lock);
	return 0;

}

static int _dbg_get_mod_debug_level = 0;
int dbg_get_mod_debug_level(char *mname, int mnlen, int *mlevel)
{
	unsigned int idx;
	unsigned int hid;
	dbg_mod_level_t *it;
	/* no LOG*() usage in this function and those executed insite it
	 * - use fprintf(stderr, ...) if need for troubleshooting
	 * - it will loop otherwise */
	if(_dbg_mod_table==NULL)
		return -1;

	if(cfg_get(dbg, dbg_cfg, mod_level_mode)==0)
		return -1;

	if(_dbg_get_mod_debug_level!=0)
		return -1;
	_dbg_get_mod_debug_level = 1;

	hid = dbg_compute_hash(mname, mnlen);
	idx = hid&(_dbg_mod_table_size-1);
	lock_get(&_dbg_mod_table[idx].lock);
	it = _dbg_mod_table[idx].first;
	while(it!=NULL && it->hashid < hid)
		it = it->next;
	while(it!=NULL && it->hashid == hid)
	{
		if(mnlen==it->name.len
				&& strncmp(mname, it->name.s, mnlen)==0)
		{
			/* found */
			*mlevel = it->level;
			lock_release(&_dbg_mod_table[idx].lock);
			_dbg_get_mod_debug_level = 0;
			return 0;
		}
		it = it->next;
	}
	lock_release(&_dbg_mod_table[idx].lock);
	_dbg_get_mod_debug_level = 0;
	return -1;
}

/**
 *
 */
void dbg_enable_mod_levels(void)
{
	if(_dbg_mod_table==NULL)
		return;
	set_module_debug_level_cb(dbg_get_mod_debug_level);
}

#define DBG_PVCACHE_SIZE 32

typedef struct _dbg_pvcache {
	pv_spec_t *spec;
	str *pvname;
	struct _dbg_pvcache *next;
} dbg_pvcache_t;

static dbg_pvcache_t **_dbg_pvcache = NULL;

int dbg_init_pvcache()
{
	_dbg_pvcache = (dbg_pvcache_t**)pkg_malloc(sizeof(dbg_pvcache_t*)*DBG_PVCACHE_SIZE);
	if(_dbg_pvcache==NULL)
	{
		LM_ERR("no more memory.\n");
		return -1;
	}
	memset(_dbg_pvcache, 0, sizeof(dbg_pvcache_t*)*DBG_PVCACHE_SIZE);
	return 0;
}

int dbg_assign_add(str *name, pv_spec_t *spec)
{
	dbg_pvcache_t *pvn, *last, *next;
	unsigned int pvid;

	if(name==NULL||spec==NULL)
		return -1;

	if(_dbg_pvcache==NULL)
		return -1;

	pvid = get_hash1_raw((char *)&spec, sizeof(pv_spec_t*));
	pvn = (dbg_pvcache_t*)pkg_malloc(sizeof(dbg_pvcache_t));
	if(pvn==NULL)
	{
		LM_ERR("no more memory\n");
		return -1;
	}
	memset(pvn, 0, sizeof(dbg_pvcache_t));
	pvn->pvname = name;
	pvn->spec = spec;
	next = _dbg_pvcache[pvid%DBG_PVCACHE_SIZE];
	if(next==NULL)
	{
		_dbg_pvcache[pvid%DBG_PVCACHE_SIZE] = pvn;
	}
	else
	{
		while(next)
		{
			last = next;
			next = next->next;
		}
		last->next = pvn;
	}
	return 0;
}

str *_dbg_pvcache_lookup(pv_spec_t *spec)
{
	dbg_pvcache_t *pvi;
	unsigned int pvid;
	str *name = NULL;

	if(spec==NULL)
		return NULL;

	if(_dbg_pvcache==NULL)
		return NULL;

	pvid = get_hash1_raw((char *)&spec, sizeof(pv_spec_t*));
	pvi = _dbg_pvcache[pvid%DBG_PVCACHE_SIZE];
	while(pvi)
	{
		if(pvi->spec==spec) {
			return pvi->pvname;
		}
		pvi = pvi->next;
	}
	name = pv_cache_get_name(spec);
	if(name!=NULL)
	{
		/*LM_DBG("Add name[%.*s] to pvcache\n", name->len, name->s);*/
		dbg_assign_add(name, spec);
	}
	return name;
}

int _dbg_log_assign_action_avp(struct sip_msg* msg, struct lvalue* lv)
{
	int_str avp_val;
	avp_t* avp;
	avp_spec_t* avp_s = &lv->lv.avps;
	avp = search_avp_by_index(avp_s->type, avp_s->name,
				&avp_val, avp_s->index);
	if (likely(avp)){
		if (avp->flags&(AVP_VAL_STR)){
			LM_DBG("%.*s:\"%.*s\"\n", avp_s->name.s.len, avp_s->name.s.s,
				avp_val.s.len, avp_val.s.s);
		}else{
			LM_DBG("%.*s:%d\n", avp_s->name.s.len, avp_s->name.s.s,
				avp_val.n);
		}
	}
	return 0;
}

int _dbg_log_assign_action_pvar(struct sip_msg* msg, struct lvalue* lv)
{
	pv_value_t value;
	pv_spec_t* pvar = lv->lv.pvs;
	str def_name = {"unknown", 7};
	str *name = _dbg_pvcache_lookup(pvar);

	if(name==NULL)
		name = &def_name;
	if(pv_get_spec_value(msg, pvar, &value)!=0)
	{
		LM_ERR("can't get value\n");
		return -1;
	}

	if(value.flags&(PV_VAL_NULL|PV_VAL_EMPTY|PV_VAL_NONE)){
		LM_DBG("%.*s: $null\n", name->len, name->s);
	}else if(value.flags&(PV_VAL_INT)){
		LM_DBG("%.*s:%d\n", name->len, name->s, value.ri);
	}else if(value.flags&(PV_VAL_STR)){
		LM_DBG("%.*s:\"%.*s\"\n", name->len, name->s, value.rs.len, value.rs.s);
	}
	return 0;
}

int dbg_log_assign(struct sip_msg* msg, struct lvalue *lv)
{
	if(lv==NULL)
	{
		LM_ERR("left value is NULL\n");
		return -1;
	}
	switch(lv->type){
		case LV_AVP:
			return _dbg_log_assign_action_avp(msg, lv);
			break;
		case LV_PVAR:
			return _dbg_log_assign_action_pvar(msg, lv);
			break;
		case LV_NONE:
			break;
	}
	return 0;
}

void dbg_enable_log_assign(void)
{
	if(_dbg_pvcache==NULL)
		return;
	set_log_assign_action_cb(dbg_log_assign);
}

int dbg_level_mode_fixup(void *temp_handle,
	str *group_name, str *var_name, void **value){
	if(_dbg_mod_table==NULL)
	{
		LM_ERR("mod_hash_size must be set on start\n");
		return -1;
	}
	return 0;
}

int _dbg_get_array_avp_vals(struct sip_msg *msg,
		pv_param_t *param, srjson_doc_t *jdoc, srjson_t **jobj,
		str *item_name)
{
	struct usr_avp *avp;
	unsigned short name_type;
	int_str avp_name;
	int_str avp_value;
	struct search_state state;
	srjson_t *jobjt;
	memset(&state, 0, sizeof(struct search_state));

	if(pv_get_avp_name(msg, param, &avp_name, &name_type)!=0)
	{
		LM_ERR("invalid name\n");
		return -1;
	}
	*jobj = srjson_CreateArray(jdoc);
	if(*jobj==NULL)
	{
		LM_ERR("cannot create json object\n");
		return -1;
	}
	if ((avp=search_first_avp(name_type, avp_name, &avp_value, &state))==0)
	{
		goto ok;
	}
	do
	{
		if(avp->flags & AVP_VAL_STR)
		{
			jobjt = srjson_CreateStr(jdoc, avp_value.s.s, avp_value.s.len);
			if(jobjt==NULL)
			{
				LM_ERR("cannot create json object\n");
				return -1;
			}
		} else {
			jobjt = srjson_CreateNumber(jdoc, avp_value.n);
			if(jobjt==NULL)
			{
				LM_ERR("cannot create json object\n");
				return -1;
			}
		}
		srjson_AddItemToArray(jdoc, *jobj, jobjt);
	} while ((avp=search_next_avp(&state, &avp_value))!=0);
ok:
	item_name->s = avp_name.s.s;
	item_name->len = avp_name.s.len;
	return 0;
}
#define DBG_XAVP_DUMP_SIZE 32
static str* _dbg_xavp_dump[DBG_XAVP_DUMP_SIZE];
int _dbg_xavp_dump_lookup(pv_param_t *param)
{
	unsigned int i = 0;
	pv_xavp_name_t *xname;

	if(param==NULL)
		return -1;

	xname = (pv_xavp_name_t*)param->pvn.u.dname;

	while(_dbg_xavp_dump[i]!=NULL&&i<DBG_XAVP_DUMP_SIZE)
	{
		if(_dbg_xavp_dump[i]->len==xname->name.len)
		{
			if(strncmp(_dbg_xavp_dump[i]->s, xname->name.s, xname->name.len)==0)
				return 1; /* already dump before */
		}
		i++;
	}
	if(i==DBG_XAVP_DUMP_SIZE)
	{
		LM_WARN("full _dbg_xavp_dump cache array\n");
		return 0; /* end cache names */
	}
	_dbg_xavp_dump[i] = &xname->name;
	return 0;
}

void _dbg_get_obj_xavp_val(sr_xavp_t *avp, srjson_doc_t *jdoc, srjson_t **jobj)
{
	static char _pv_xavp_buf[128];
	int result = 0;

	switch(avp->val.type) {
		case SR_XTYPE_NULL:
			*jobj = srjson_CreateNull(jdoc);
		break;
		case SR_XTYPE_INT:
			*jobj = srjson_CreateNumber(jdoc, avp->val.v.i);
		break;
		case SR_XTYPE_STR:
			*jobj = srjson_CreateStr(jdoc, avp->val.v.s.s, avp->val.v.s.len);
		break;
		case SR_XTYPE_TIME:
			result = snprintf(_pv_xavp_buf, 128, "%lu", (long unsigned)avp->val.v.t);
		break;
		case SR_XTYPE_LONG:
			result = snprintf(_pv_xavp_buf, 128, "%ld", (long unsigned)avp->val.v.l);
		break;
		case SR_XTYPE_LLONG:
			result = snprintf(_pv_xavp_buf, 128, "%lld", avp->val.v.ll);
		break;
		case SR_XTYPE_XAVP:
			result = snprintf(_pv_xavp_buf, 128, "<<xavp:%p>>", avp->val.v.xavp);
		break;
		case SR_XTYPE_DATA:
			result = snprintf(_pv_xavp_buf, 128, "<<data:%p>>", avp->val.v.data);
		break;
		default:
			LM_WARN("unknown data type\n");
			*jobj = srjson_CreateNull(jdoc);
	}
	if(result<0)
	{
		LM_ERR("cannot convert to str\n");
		*jobj = srjson_CreateNull(jdoc);
	}
	else if(*jobj==NULL)
	{
		*jobj = srjson_CreateStr(jdoc, _pv_xavp_buf, 128);
	}
}

int _dbg_get_obj_avp_vals(str name, sr_xavp_t *xavp, srjson_doc_t *jdoc, srjson_t **jobj)
{
	sr_xavp_t *avp = NULL;
	srjson_t *jobjt = NULL;

	*jobj = srjson_CreateArray(jdoc);
	if(*jobj==NULL)
	{
		LM_ERR("cannot create json object\n");
		return -1;
	}
	avp = xavp;
	while(avp!=NULL&&!STR_EQ(avp->name,name))
	{
		avp = avp->next;
	}
	while(avp!=NULL)
	{
		_dbg_get_obj_xavp_val(avp, jdoc, &jobjt);
		srjson_AddItemToArray(jdoc, *jobj, jobjt);
		jobjt = NULL;
		avp = xavp_get_next(avp);
	}

	return 0;
}

int _dbg_get_obj_xavp_vals(struct sip_msg *msg,
		pv_param_t *param, srjson_doc_t *jdoc, srjson_t **jobjr,
		str *item_name)
{
	pv_xavp_name_t *xname = (pv_xavp_name_t*)param->pvn.u.dname;
	sr_xavp_t *xavp = NULL;
	sr_xavp_t *avp = NULL;
	srjson_t *jobj = NULL;
	srjson_t *jobjt = NULL;
	struct str_list *keys;
	struct str_list *k;

	*jobjr = srjson_CreateArray(jdoc);
	if(*jobjr==NULL)
	{
		LM_ERR("cannot create json object\n");
		return -1;
	}

	item_name->s = xname->name.s;
	item_name->len = xname->name.len;
	xavp = xavp_get_by_index(&xname->name, 0, NULL);
	if(xavp==NULL)
	{
		return 0; /* empty */
	}

	do
	{
		if(xavp->val.type==SR_XTYPE_XAVP)
		{
			avp = xavp->val.v.xavp;
			jobj = srjson_CreateObject(jdoc);
			if(jobj==NULL)
			{
				LM_ERR("cannot create json object\n");
				return -1;
			}
			keys = xavp_get_list_key_names(xavp);
			if(keys!=NULL)
			{
				do
				{
					_dbg_get_obj_avp_vals(keys->s, avp, jdoc, &jobjt);
					srjson_AddStrItemToObject(jdoc, jobj, keys->s.s,
						keys->s.len, jobjt);
					k = keys;
					keys = keys->next;
					pkg_free(k);
					jobjt = NULL;
				}while(keys!=NULL);
			}
		}
		if(jobj!=NULL)
		{
			srjson_AddItemToArray(jdoc, *jobjr, jobj);
			jobj = NULL;
		}
	}while((xavp = xavp_get_next(xavp))!=0);

	return 0;
}

int dbg_dump_json(struct sip_msg* msg, unsigned int mask, int level)
{
	int i;
	pv_value_t value;
	pv_cache_t **_pv_cache = pv_cache_get_table();
	pv_cache_t *el = NULL;
	srjson_doc_t jdoc;
	srjson_t *jobj = NULL;
	char *output = NULL;
	str item_name = STR_NULL;
	static char iname[128];
	int result = -1;

	if(_pv_cache==NULL)
	{
		LM_ERR("cannot access pv_cache\n");
		return -1;
	}

	memset(_dbg_xavp_dump, 0, sizeof(str*)*DBG_XAVP_DUMP_SIZE);
	srjson_InitDoc(&jdoc, NULL);
	if(jdoc.root==NULL)
	{
		jdoc.root = srjson_CreateObject(&jdoc);
		if(jdoc.root==NULL)
		{
			LM_ERR("cannot create json root\n");
			goto error;
		}
	}
	for(i=0;i<PV_CACHE_SIZE;i++)
	{
		el = _pv_cache[i];
		while(el)
		{
			if(!(el->spec.type==PVT_AVP||
				el->spec.type==PVT_SCRIPTVAR||
				el->spec.type==PVT_XAVP||
				el->spec.type==PVT_OTHER)||
				!((el->spec.type==PVT_AVP&&mask&DBG_DP_AVP)||
				(el->spec.type==PVT_XAVP&&mask&DBG_DP_XAVP)||
				(el->spec.type==PVT_SCRIPTVAR&&mask&DBG_DP_SCRIPTVAR)||
				(el->spec.type==PVT_OTHER&&mask&DBG_DP_OTHER))||
				(el->spec.trans!=NULL))
			{
				el = el->next;
				continue;
			}
			jobj = NULL;
			item_name.len = 0;
			item_name.s = 0;
			iname[0] = '\0';
			if(el->spec.type==PVT_AVP)
			{
				if(el->spec.pvp.pvi.type==PV_IDX_ALL||
					(el->spec.pvp.pvi.type==PV_IDX_INT&&el->spec.pvp.pvi.u.ival!=0))
				{
					el = el->next;
					continue;
				}
				else
				{
					if(_dbg_get_array_avp_vals(msg, &el->spec.pvp, &jdoc, &jobj, &item_name)!=0)
					{
						LM_WARN("can't get value[%.*s]\n", el->pvname.len, el->pvname.s);
						el = el->next;
						continue;
					}
					if(srjson_GetArraySize(&jdoc, jobj)==0 && !(mask&DBG_DP_NULL))
					{
						el = el->next;
						continue;
					}
					snprintf(iname, 128, "$avp(%.*s)", item_name.len, item_name.s);
				}
			}
			else if(el->spec.type==PVT_XAVP)
			{
				if(_dbg_xavp_dump_lookup(&el->spec.pvp)!=0)
				{
					el = el->next;
					continue;
				}
				if(_dbg_get_obj_xavp_vals(msg, &el->spec.pvp, &jdoc, &jobj, &item_name)!=0)
				{
					LM_WARN("can't get value[%.*s]\n", el->pvname.len, el->pvname.s);
					el = el->next;
					continue;
				}
				if(srjson_GetArraySize(&jdoc, jobj)==0 && !(mask&DBG_DP_NULL))
				{
					el = el->next;
					continue;
				}
				snprintf(iname, 128, "$xavp(%.*s)", item_name.len, item_name.s);
			}
			else
			{
				if(pv_get_spec_value(msg, &el->spec, &value)!=0)
				{
					LM_WARN("can't get value[%.*s]\n", el->pvname.len, el->pvname.s);
					el = el->next;
					continue;
				}
				if(value.flags&(PV_VAL_NULL|PV_VAL_EMPTY|PV_VAL_NONE))
				{
					if(mask&DBG_DP_NULL)
					{
						jobj = srjson_CreateNull(&jdoc);
					}
					else
					{
						el = el->next;
						continue;
					}
				}else if(value.flags&(PV_VAL_INT)){
					jobj = srjson_CreateNumber(&jdoc, value.ri);
				}else if(value.flags&(PV_VAL_STR)){
					jobj = srjson_CreateStr(&jdoc, value.rs.s, value.rs.len);
				}else {
					LM_WARN("el->pvname[%.*s] value[%d] unhandled\n", el->pvname.len, el->pvname.s,
						value.flags);
					el = el->next;
					continue;
				}
				if(jobj==NULL)
				{
					LM_ERR("el->pvname[%.*s] empty json object\n", el->pvname.len,
						el->pvname.s);
					goto error;
				}
				snprintf(iname, 128, "%.*s", el->pvname.len, el->pvname.s);
			}
			if(jobj!=NULL)
			{
				srjson_AddItemToObject(&jdoc, jdoc.root, iname, jobj);
			}
			el = el->next;
		}
	}
	output = srjson_PrintUnformatted(&jdoc, jdoc.root);
	if(output==NULL)
	{
		LM_ERR("cannot print json doc\n");
		goto error;
	}
	LOG(level, "%s\n", output);
	result = 0;

error:
	if(output!=NULL) jdoc.free_fn(output);
	srjson_DestroyDoc(&jdoc);

	return result;
}
