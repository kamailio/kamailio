/*
 * $Id$
 *
 * Copyright (C) 2006 Voice Sistem SRL
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
 * History:
 * ---------
 *  2006-01-23  first version (bogdan)
 *  2006-11-28  Added statistics for the number of bad URI's, methods, and 
 *              proxy requests (Jeffrey Magder - SOMA Networks)
 */

/*!
 * \file
 * \brief Kamailio Core statistics
 */


#include <string.h>

#include "../../lib/kcore/statistics.h"
#include "../../lib/kmi/mi.h"
#include "../../dprint.h"


#ifdef STATISTICS

stat_var* rcv_reqs;				/*!< received requests        */
stat_var* rcv_rpls;				/*!< received replies         */
stat_var* fwd_reqs;				/*!< forwarded requests       */
stat_var* fwd_rpls;				/*!< forwarded replies        */
stat_var* drp_reqs;				/*!< dropped requests         */
stat_var* drp_rpls;				/*!< dropped replies          */
stat_var* err_reqs;				/*!< error requests           */
stat_var* err_rpls;				/*!< error replies            */
stat_var* bad_URIs;				/*!< number of bad URIs       */
stat_var* unsupported_methods;	/*!< unsupported methods      */
stat_var* bad_msg_hdr;			/*!< messages with bad header */


/*! exported core statistics */
stat_export_t core_stats[] = {
	{"rcv_requests" ,         0,  &rcv_reqs              },
	{"rcv_replies" ,          0,  &rcv_rpls              },
	{"fwd_requests" ,         0,  &fwd_reqs              },
	{"fwd_replies" ,          0,  &fwd_rpls              },
	{"drop_requests" ,        0,  &drp_reqs              },
	{"drop_replies" ,         0,  &drp_rpls              },
	{"err_requests" ,         0,  &err_reqs              },
	{"err_replies" ,          0,  &err_rpls              },
	{"bad_URIs_rcvd",         0,  &bad_URIs              },
	{"unsupported_methods",   0,  &unsupported_methods   },
	{"bad_msg_hdr",           0,  &bad_msg_hdr           },
	{0,0,0}
};

static struct mi_root *mi_get_stats(struct mi_root *cmd, void *param);
static struct mi_root *mi_reset_stats(struct mi_root *cmd, void *param);

static mi_export_t mi_stat_cmds[] = {
	{ "get_statistics",    mi_get_stats,    0  ,  0,  0 },
	{ "reset_statistics",  mi_reset_stats,  0  ,  0,  0 },
	{ 0, 0, 0, 0, 0}
};

int register_mi_stats(void)
{
	/* register MI commands */
	if (register_mi_mod( "statistics", mi_stat_cmds)<0) {
		LM_ERR("unable to register MI cmds\n");
		return -1;
	}
	return 0;
}
int register_core_stats(void)
{
	/* register core statistics */
	if (register_module_stats( "core", core_stats)!=0 ) {
		LM_ERR("failed to register core statistics\n");
		return -1;
	}
#if 0
	/* register sh_mem statistics */
	if (register_module_stats( "shmem", shm_stats)!=0 ) {
		LM_ERR("failed to register sh_mem statistics\n");
		return -1;
	}
#endif
	return 0;
}

/***************************** MI STUFF ********************************/

inline static int mi_add_stat(struct mi_node *rpl, stat_var *stat)
{
	struct mi_node *node;
	stats_collector *sc;

	if((sc = get_stats_collector())==NULL) return -1;

	node = addf_mi_node_child(rpl, 0, 0, 0, "%.*s:%.*s = %lu",
		sc->amodules[stat->mod_idx].name.len,
		sc->amodules[stat->mod_idx].name.s,
		stat->name.len, stat->name.s,
		get_stat_val(stat) );

	if (node==0)
		return -1;
	return 0;
}

inline static int mi_add_module_stats(struct mi_node *rpl,
													module_stats *mods)
{
	struct mi_node *node;
	stat_var *stat;

	for( stat=mods->head ; stat ; stat=stat->lnext) {
		node = addf_mi_node_child(rpl, 0, 0, 0, "%.*s:%.*s = %lu",
			mods->name.len, mods->name.s,
			stat->name.len, stat->name.s,
			get_stat_val(stat) );
		if (node==0)
			return -1;
	}
	return 0;
}


static struct mi_root *mi_get_stats(struct mi_root *cmd, void *param)
{
	struct mi_root *rpl_tree;
	struct mi_node *rpl;
	struct mi_node *arg;
	module_stats   *mods;
	stat_var       *stat;
	str val;
	int i;

	stats_collector *sc;

	if((sc = get_stats_collector())==NULL)
		return init_mi_tree( 404, "Statistics Not Found", 20);

	if (cmd->node.kids==NULL)
		return init_mi_tree( 400, MI_MISSING_PARM_S, MI_MISSING_PARM_LEN);

	rpl_tree = init_mi_tree( 200, MI_OK_S, MI_OK_LEN);
	if (rpl_tree==0)
		return 0;
	rpl = &rpl_tree->node;

	for( arg=cmd->node.kids ; arg ; arg=arg->next) {
		if (arg->value.len==0)
			continue;

		val = arg->value;

		if ( val.len==3 && memcmp(val.s,"all",3)==0) {
			/* add all statistic variables */
			for( i=0 ; i<sc->mod_no ;i++ ) {
				if (mi_add_module_stats( rpl, &sc->amodules[i] )!=0)
					goto error;
			}
		} else if ( val.len>1 && val.s[val.len-1]==':') {
			/* add module statistics */
			val.len--;
			mods = get_stat_module( &val );
			if (mods==0)
				continue;
			if (mi_add_module_stats( rpl, mods )!=0)
				goto error;
		} else {
			/* add only one statistic */
			stat = get_stat( &val );
			if (stat==0)
				continue;
			if (mi_add_stat(rpl,stat)!=0)
				goto error;
		}
	}

	if (rpl->kids==0) {
		free_mi_tree(rpl_tree);
		return init_mi_tree( 404, "Statistics Not Found", 20);
	}

	return rpl_tree;
error:
	free_mi_tree(rpl_tree);
	return 0;
}



static struct mi_root *mi_reset_stats(struct mi_root *cmd, void *param)
{
	struct mi_root *rpl_tree;
	struct mi_node *arg;
	stat_var       *stat;
	int found;

	if (cmd->node.kids==NULL)
		return init_mi_tree( 400, MI_MISSING_PARM_S, MI_MISSING_PARM_LEN);

	rpl_tree = init_mi_tree( 200, MI_OK_S, MI_OK_LEN);
	if (rpl_tree==0)
		return 0;
	found = 0;

	for( arg=cmd->node.kids ; arg ; arg=arg->next) {
		if (arg->value.len==0)
			continue;

		stat = get_stat( &arg->value );
		if (stat==0)
			continue;

		reset_stat( stat );
		found = 1;
	}

	if (!found) {
		free_mi_tree(rpl_tree);
		return init_mi_tree( 404, "Statistics Not Found", 20);
	}

	return rpl_tree;
}

#endif
