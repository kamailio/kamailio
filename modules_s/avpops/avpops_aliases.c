/*
 * $Id$
 *
 * Copyright (C) 2004 Voice Sistem SRL
 *
 * This file is part of SIP Express Router.
 *
 * AVPOPS SER-module is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License
 * as published by the Free Software Foundation; either version 2
 * of the License, or (at your option) any later version.
 *
 * AVPOPS SER-module is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 59 Temple Place - Suite 330, Boston, MA  02111-1307, USA.
 *
 * For any questions about this software and its license, please contact
 * Voice Sistem at following e-mail address:
 *         office@voice-sistem.ro
 *
 *
 * History:
 * ---------
 *  2004-10-04  first version (ramona)
 */



#include "../../dprint.h"
#include "../../mem/mem.h"
#include "avpops_aliases.h"



struct avp_alias
{
	char *alias;  /* null terminated */
	struct fis_param *attr; /*attribute name as flag+int_str_val*/
	struct avp_alias *next;
};


static struct avp_alias *aliases = 0;


int add_avp_alias(char* alias, struct fis_param *attr)
{
	struct avp_alias *aa;

	/* check parameters */
	if (alias==0 || strlen(alias)==0 || attr==0)
	{
		LOG(L_ERR,"ERROR:avpops:add_avp_alias: invalid parameters\n");
		goto error;
	}

	/* check for overlapping aliases */
	for(aa=aliases;aa;aa=aa->next)
		if ( strcasecmp(alias,aa->alias)==0)
		{
			LOG(L_ERR,"ERROR:avpops:add_avp_alias: alias <%s> already"
				"defined\n",alias);
			goto error;
		}

	aa = (struct avp_alias*)pkg_malloc( sizeof(struct avp_alias) );
	if (aa==0)
	{
		LOG(L_ERR,"ERROR:avpops:add_avp_alias: no more pkg mem\n" );
		goto error;
	}
	aa->alias = alias;
	aa->attr = attr;

	attr->flags |= AVPOPS_VAL_AVP;

	aa->next = aliases;
	aliases = aa;
	if (attr->flags&AVPOPS_VAL_INT)
		DBG("DEBUG:avpops:add_avp_alias: added <%s>=%d\n",alias,attr->val.n);
	else
		DBG("DEBUG:avpops:add_avp_alias: added <%s>=<%s>\n",alias,
			attr->val.s->s);

	return 0;
error:
	return -1;
}


struct fis_param *lookup_avp_alias( char *alias)
{
	struct avp_alias *aa;

	if (alias==0 || strlen(alias)==0)
	{
		LOG(L_ERR,"ERROR:avpops:lookup_avp_alias: invalid parameters\n");
		goto error;
	}

	for(aa=aliases;aa;aa=aa->next)
		if (strcasecmp(alias,aa->alias)==0 )
			return aa->attr;

error:
	return 0;
}


void destroy_avp_aliases()
{
	struct avp_alias *aa;

	while(aliases)
	{
		aa = aliases;
		aliases = aa->next;
		pkg_free(aa);
	}
	aliases = 0;
}

