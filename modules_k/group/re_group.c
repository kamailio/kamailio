/*
 * $Id$ 
 *
 * Copyright (C) 2005-2007 Voice Sistem SRL
 *
 * This file is part of openser, a free SIP server.
 *
 * openser is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version
 *
 * openser is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License 
 * along with this program; if not, write to the Free Software 
 * Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307  USA
 *
 * History:
 * --------
 *  2005-10-06 - created by bogdan
 */

#include <sys/types.h>
#include <regex.h>

#include "../../str.h"
#include "../../mem/mem.h"
#include "group_mod.h"
#include "re_group.h"
#include "group.h"


struct re_grp {
	regex_t       re;
	int_str       gid;
	struct re_grp *next;
};


static struct re_grp *re_list = 0;


static int add_re(char *re, int gid)
{
	struct re_grp *rg;

	DBG("DEBUG:group:add_re: adding <%s> with %d\n",re, gid);

	rg = (struct re_grp*)pkg_malloc(sizeof(struct re_grp));
	if (rg==0) {
		LOG(L_ERR,"ERROR:group:add_re: no more pkg mem\n");
		goto error;
	}
	memset( rg, 0, sizeof(struct re_grp));

	if (regcomp(&rg->re, re, REG_EXTENDED|REG_ICASE|REG_NEWLINE) ) {
		LOG(L_ERR, "ERROR:group:add_re: bad re %s\n", re);
		pkg_free(rg);
		goto error;
	}

	rg->gid.n = gid;

	rg->next = re_list;
	re_list = rg;

	return 0;
error:
	return -1;
}



int load_re( str *table )
{
	db_key_t cols[2];
	db_res_t* res = NULL;
	db_row_t* row;
	int n;

	cols[0] = re_exp_column.s;
	cols[1] = re_gid_column.s;

	if (group_dbf.use_table(group_dbh, table->s) < 0) {
		LOG(L_ERR, "ERROR:group:load_re: failed to set table <%s>\n",
			table->s);
		goto error;
	}

	if (group_dbf.query(group_dbh, 0, 0, 0, cols, 0, 2, 0, &res) < 0) {
		LOG(L_ERR, "ERROR:group:load_re: Error while querying database\n");
		goto error;
	}

	for( n=0 ; n<RES_ROW_N(res) ; n++) {
		row = &res->rows[n];
		/* validate row */
		if (row->values[0].nul || row->values[0].type!=DB_STRING) {
			LOG(L_ERR,"ERROR:group:load_re: empty or non-string "
				"value for <%s>(re) column\n",re_exp_column.s);
			goto error1;
		}
		if (row->values[1].nul || row->values[1].type!=DB_INT) {
			LOG(L_ERR,"ERROR:group:load_re: empty or non-integer "
				"value for <%s>(gid) column\n",re_gid_column.s);
			goto error1;
		}

		if ( add_re( (char*)row->values[0].val.string_val,
		row->values[1].val.int_val)!=0 ) {
			LOG(L_ERR,"ERROR:group:load_re: failed to add row\n");
			goto error1;
		}
	}
	DBG("DEBUG:group:load_re: %d rules were loaded\n", n);

	group_dbf.free_result(group_dbh, res);
	return 0;
error1:
	group_dbf.free_result(group_dbh, res);
error:
	return -1;
}



int get_user_group(struct sip_msg *req, char *user, char *avp)
{
	static char uri_buf[MAX_URI_SIZE];
	str  username;
	str  domain;
	struct gid_spec *gs;
	struct re_grp *rg;
	regmatch_t pmatch;
	char *c;
	int n;

	if (get_username_domain( req, (group_check_p)user, &username, &domain)!=0){
		LOG(L_ERR, "ERROR:group:get_user_group: failed to get "
			"username@domain\n");
		goto error;
	}

	if ( 4 + username.len + 1 + domain.len + 1 > MAX_URI_SIZE ) {
		LOG(L_ERR, "ERROR:group:get_user_group: URI to large!!\n");
		goto error;
	}

	*(int*)uri_buf = htonl(('s'<<24) + ('i'<<16) + ('p'<<8) + ':');
	c = uri_buf + 4;
	memcpy( c, username.s, username.len);
	c += username.len;
	*(c++) = '@';
	memcpy( c, domain.s, domain.len);
	c += domain.len;
	*c = 0;

	DBG("DEBUG:group:get_user_group: getting groups for <%s>\n",uri_buf);
	gs = (struct gid_spec*)avp;

	/* check against all re groups */
	for( rg=re_list,n=0 ; rg ; rg=rg->next ) {
		if (regexec( &rg->re, uri_buf, 1, &pmatch, 0)==0) {
			DBG("DEBUG:group:get_user_group: user matched to group %d!\n",
				rg->gid.n);
			/* match -> add the gid as AVP */
			if ( add_avp( (unsigned short)gs->avp_type, gs->avp_name,
			rg->gid )!= 0 ) {
				LOG(L_ERR, "ERROR:group:get_user_group: failed to add avp\n");
				goto error;
			}
			n++;
			/* continue? */
			if (multiple_gid==0)
				break;
		}
	}

	return n?n:-1;
error:
	return -1;
}


