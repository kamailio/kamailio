/*
 * Copyright (C) 2010 Daniel-Constantin Mierla (asipto.com)
 *
 * This file is part of Kamailio, a free SIP server.
 *
 * Kamailio is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * Kamailio is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 59 Temple Place - Suite 330, Boston, MA  02111-1307, USA.
 *
 */

/*!
 * \file
 * \brief KEX :: Kamailio private memory pool statistics
 * \ingroup kex
 */

#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <unistd.h>

#include "../../dprint.h"
#include "../../ut.h"
#include "../../pt.h"
#include "../../events.h"
#include "../../mem/mem.h"
#include "../../mem/shm_mem.h"
#include "../../rpc.h"
#include "../../rpc_lookup.h"


/**
 *
 */
typedef struct pkg_proc_stats {
	int rank;
	unsigned int pid;
	unsigned int used;
	unsigned int available;
	unsigned int real_used;
} pkg_proc_stats_t;

/**
 *
 */
static pkg_proc_stats_t *_pkg_proc_stats_list = NULL;

/**
 *
 */
static int _pkg_proc_stats_no = 0;

/**
 *
 */
int pkg_proc_stats_init(void)
{
	_pkg_proc_stats_no = get_max_procs();

	if(_pkg_proc_stats_no<=0)
		return -1;
	if(_pkg_proc_stats_list!=NULL)
		return -1;
	_pkg_proc_stats_list = (pkg_proc_stats_t*)shm_malloc(
			_pkg_proc_stats_no*sizeof(pkg_proc_stats_t));
	if(_pkg_proc_stats_list==NULL)
		return -1;
	memset(_pkg_proc_stats_list, 0,
			_pkg_proc_stats_no*sizeof(pkg_proc_stats_t));
	return 0;
}

/**
 *
 */
int pkg_proc_stats_myinit(int rank)
{
	struct mem_info info;
	if(_pkg_proc_stats_list==NULL)
		return -1;
	if(process_no>=_pkg_proc_stats_no)
		return -1;
	_pkg_proc_stats_list[process_no].pid = (unsigned int)my_pid();
	_pkg_proc_stats_list[process_no].rank = rank;

	/* init pkg usage values */
	pkg_info(&info);
	_pkg_proc_stats_list[process_no].used = info.used;
	_pkg_proc_stats_list[process_no].real_used = info.real_used;
	return 0;
}

/**
 *
 */
int pkg_proc_stats_destroy(void)
{
	if(_pkg_proc_stats_list==NULL)
		return -1;
	shm_free(_pkg_proc_stats_list);
	_pkg_proc_stats_list = 0;
	_pkg_proc_stats_no = 0;
	return 0;
}


/**
 *
 */
static int pkg_proc_update_used(void *data)
{
	if(_pkg_proc_stats_list==NULL)
		return -1;
	if(process_no>=_pkg_proc_stats_no)
		return -1;
	_pkg_proc_stats_list[process_no].used = (unsigned int)(long)data;
	return 0;
}

/**
 *
 */
static int pkg_proc_update_real_used(void *data)
{
	if(_pkg_proc_stats_list==NULL)
		return -1;
	if(process_no>=_pkg_proc_stats_no)
		return -1;
	_pkg_proc_stats_list[process_no].real_used = (unsigned int)(long)data;
	_pkg_proc_stats_list[process_no].available = pkg_available();
	return 0;
}

/**
 *
 */
int register_pkg_proc_stats(void)
{
	sr_event_register_cb(SREV_PKG_SET_USED, pkg_proc_update_used);
	sr_event_register_cb(SREV_PKG_SET_REAL_USED, pkg_proc_update_real_used);
	return 0;
}

/**
 *
 */
static const char* rpc_pkg_stats_doc[2] = {
	"Private memory (pkg) statistics per process",
	0
};

/**
 *
 */
int pkg_proc_get_pid_index(unsigned int pid)
{
	int i;
	for(i=0; i<_pkg_proc_stats_no; i++)
	{
		if(_pkg_proc_stats_list[i].pid == pid)
			return i;
	}
	return -1;
}

/**
 *
 */
static void rpc_pkg_stats(rpc_t* rpc, void* ctx)
{
	int i;
	int limit;
	int cval;
	str cname;
	void* th;
	int mode;

	if(_pkg_proc_stats_list==NULL)
	{
		rpc->fault(ctx, 500, "Not initialized");
		return;
	}
	i = 0;
	mode = 0;
	cval = 0;
	limit = _pkg_proc_stats_no;
	if (rpc->scan(ctx, "*S", &cname) == 1)
	{
		if(cname.len==3 && strncmp(cname.s, "pid", 3)==0)
			mode = 1;
		else if(cname.len==4 && strncmp(cname.s, "rank", 4)==0)
			mode = 2;
		else if(cname.len==5 && strncmp(cname.s, "index", 5)==0)
			mode = 3;
		else {
			rpc->fault(ctx, 500, "Invalid filter type");
			return;
		}

		if (rpc->scan(ctx, "d", &cval) < 1)
		{
			rpc->fault(ctx, 500, "One more parameter expected");
			return;
		}
		if(mode==1)
		{
			i = pkg_proc_get_pid_index((unsigned int)cval);
			if(i<0)
			{
				rpc->fault(ctx, 500, "No such pid");
				return;
			}
			limit = i + 1;
		} else if(mode==3) {
			i=cval;
			limit = i + 1;
		}
	}

	for(; i<limit; i++)
	{
		/* add entry node */
		if(mode!=2 || _pkg_proc_stats_list[i].rank==cval)
		{
			if (rpc->add(ctx, "{", &th) < 0)
			{
				rpc->fault(ctx, 500, "Internal error creating rpc");
				return;
			}
			if(rpc->struct_add(th, "dddddd",
							"entry",     i,
							"pid",       _pkg_proc_stats_list[i].pid,
							"rank",      _pkg_proc_stats_list[i].rank,
							"used",      _pkg_proc_stats_list[i].used,
							"free",      _pkg_proc_stats_list[i].available,
							"real_used", _pkg_proc_stats_list[i].real_used
						)<0)
			{
				rpc->fault(ctx, 500, "Internal error creating rpc");
				return;
			}
		}
	}
}

/**
 *
 */
rpc_export_t kex_pkg_rpc[] = {
	{"pkg.stats", rpc_pkg_stats,  rpc_pkg_stats_doc,       RET_ARRAY},
	{0, 0, 0, 0}
};

/**
 *
 */
int pkg_proc_stats_init_rpc(void)
{
	if (rpc_register_array(kex_pkg_rpc)!=0)
	{
		LM_ERR("failed to register RPC commands\n");
		return -1;
	}
	return 0;
}

