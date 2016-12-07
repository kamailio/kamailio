/*
 * Copyright (C) 2005-2007 Voice Sistem SRL
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

/**
 * \file
 * \brief Group membership module
 * \ingroup group
 * - Module: \ref group
 */

#include <sys/types.h>
#include <regex.h>

#include "../../str.h"
#include "../../mem/mem.h"
#include "../../route_struct.h"
#include "../../lvalue.h"
#include "group_mod.h"
#include "re_group.h"
#include "group.h"

/*! regular expression for groups */
struct re_grp {
	regex_t       re;
	int_str       gid;
	struct re_grp *next;
};

/*! global regexp list */
static struct re_grp *re_list = 0;


/*!
 * \brief Create a group regexp and add it to the list
 * \param re regular expression string
 * \param gid group ID
 * \return 0 on success, -1 on failure
 */
static int add_re(const char *re, int gid)
{
	struct re_grp *rg;

	LM_DBG("adding <%s> with %d\n",re, gid);

	rg = (struct re_grp*)pkg_malloc(sizeof(struct re_grp));
	if (rg==0) {
		LM_ERR("no more pkg mem\n");
		goto error;
	}
	memset( rg, 0, sizeof(struct re_grp));

	if (regcomp(&rg->re, re, REG_EXTENDED|REG_ICASE|REG_NEWLINE) ) {
		LM_ERR("bad re %s\n", re);
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


/*!
 * \brief Load regular expression rules from a database
 * \param table DB table
 * \return 0 on success, -1 on failure
 */
int load_re( str *table )
{
	db_key_t cols[2];
	db1_res_t* res = NULL;
	db_row_t* row;
	int n;

	cols[0] = &re_exp_column;
	cols[1] = &re_gid_column;

	if (group_dbf.use_table(group_dbh, table) < 0) {
		LM_ERR("failed to set table <%s>\n", table->s);
		goto error;
	}

	if (group_dbf.query(group_dbh, 0, 0, 0, cols, 0, 2, 0, &res) < 0) {
		LM_ERR("failed to query database\n");
		goto error;
	}

	for( n=0 ; n<RES_ROW_N(res) ; n++) {
		row = &res->rows[n];
		/* validate row */
		if (row->values[0].nul || row->values[0].type!=DB1_STRING) {
			LM_ERR("empty or non-string "
				"value for <%s>(re) column\n",re_exp_column.s);
			goto error1;
		}
		if (row->values[1].nul || row->values[1].type!=DB1_INT) {
			LM_ERR("empty or non-integer "
				"value for <%s>(gid) column\n",re_gid_column.s);
			goto error1;
		}

		if ( add_re( row->values[0].val.string_val,
		row->values[1].val.int_val)!=0 ) {
			LM_ERR("failed to add row\n");
			goto error1;
		}
	}
	LM_DBG("%d rules were loaded\n", n);

	group_dbf.free_result(group_dbh, res);
	return 0;
error1:
	group_dbf.free_result(group_dbh, res);
error:
	return -1;
}


/*!
 * \brief Get the user group and compare to the regexp list
 * \param req SIP message
 * \param user user string
 * \param avp AVP value
 * \return number of all matches (positive), -1 on errors or when not found
 */
int get_user_group(struct sip_msg *req, char *user, char *avp)
{
	static char uri_buf[MAX_URI_SIZE];
	str  username;
	str  domain;
	pv_spec_t *pvs;
	pv_value_t val;
	struct re_grp *rg;
	regmatch_t pmatch;
	char *c;
	int n;
	int* pi;

	if (get_username_domain( req, (group_check_p)user, &username, &domain)!=0){
		LM_ERR("failed to get username@domain\n");
		goto error;
	}

	if (username.s==NULL || username.len==0 ) {
		LM_DBG("no username part\n");
		return -1;
	}

	if ( 4 + username.len + 1 + domain.len + 1 > MAX_URI_SIZE ) {
		LM_ERR("URI to large!!\n");
		goto error;
	}

	pi=(int*)uri_buf;
	*pi = htonl(('s'<<24) + ('i'<<16) + ('p'<<8) + ':');
	c = uri_buf + 4;
	memcpy( c, username.s, username.len);
	c += username.len;
	*(c++) = '@';
	memcpy( c, domain.s, domain.len);
	c += domain.len;
	*c = 0;

	LM_DBG("getting groups for <%s>\n",uri_buf);
	pvs = (pv_spec_t*)avp;
	memset(&val, 0, sizeof(pv_value_t));
	val.flags = PV_VAL_INT|PV_TYPE_INT;

	/* check against all re groups */
	for( rg=re_list,n=0 ; rg ; rg=rg->next ) {
		if (regexec( &rg->re, uri_buf, 1, &pmatch, 0)==0) {
			LM_DBG("user matched to group %d!\n", rg->gid.n);

			/* match -> add the gid as AVP */
			val.ri = rg->gid.n;
			if(pvs->setf(req, &pvs->pvp, (int)EQ_T, &val)<0)
			{
				LM_ERR("setting PV AVP failed\n");
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


