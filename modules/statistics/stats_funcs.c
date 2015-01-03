/*
 * statistics module - script interface to internal statistics manager
 *
 * Copyright (C) 2006 Voice Sistem S.R.L.
 *
 * This file is part of Kamailio, a free SIP server.
 *
 * Kamailio is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version
 *
 * Kamailio is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License 
 * along with this program; if not, write to the Free Software 
 * Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA  02110-1301  USA
 *
 */
/*!
 * \brief Script interface
 * \ingroup statistics
 * \author bogdan
 */



#include <string.h>

#include "../../dprint.h"
#include "../../lib/kcore/statistics.h"
#include "../../mem/mem.h"
#include "stats_funcs.h"


#define NORESET_FLAG_STR "no_reset"
#define MODULE_STATS     "script"


typedef struct stat_mod_elem_
{
	char *name;
	int flags;
	struct stat_mod_elem_ *next;
} stat_elem;

static stat_elem *stat_list = 0;


int reg_statistic( char* name)
{
	stat_elem *se;
	char *flag_str;
	int flags;

	if (name==0 || *name==0) {
		LM_ERR("empty parameter\n");
		goto error;
	}

	flags = 0;
	flag_str = strchr( name, '/');
	if (flag_str) {
		*flag_str = 0;
		flag_str++;
		if (strcasecmp( flag_str, NORESET_FLAG_STR)==0) {
			flags |= STAT_NO_RESET;
		} else {
			LM_ERR("unsuported flag <%s>\n",flag_str);
			goto error;
		}
	}

	se = (stat_elem*)pkg_malloc( sizeof(stat_elem) );
	if (se==0) {
		LM_ERR("no more pkg mem\n");
		goto error;
	}

	se->name = name;
	se->flags = flags;
	se->next = stat_list;
	stat_list = se;

	return 0;
error:
	return -1;
}



int register_all_mod_stats(void)
{
	stat_var  *stat;
	stat_elem *se;
	stat_elem *se_tmp;

	se = stat_list;
	stat = NULL;
	while( se ) {
		se_tmp = se;
		se = se->next;

		/* register the new variable */
		if (register_stat(MODULE_STATS, se_tmp->name, &stat, se_tmp->flags)!=0){
			LM_ERR("failed to register var. <%s> flags %d\n",
					se_tmp->name,se_tmp->flags);
			return -1;
		}
		pkg_free(se_tmp);
	}

	return 0;
}










