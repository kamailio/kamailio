/*
 * $Id: matrix.c 4978 2008-09-23 14:25:02Z henningw $
 *
 * Copyright (C) 2007 1&1 Internet AG
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
 */

#include <string.h>

#include "../../mem/shm_mem.h"
#include "../../sr_module.h"
#include "../../lib/kmi/mi.h"
#include "../../mem/mem.h"
#include "../../usr_avp.h"
#include "../../locking.h"
#include "../../error.h"
#include "../../ut.h"
#include "../../mod_fix.h"

#include "db_matrix.h"

MODULE_VERSION




#define MAXCOLS 1000




str matrix_db_url = str_init(DEFAULT_RODB_URL);




/**
 * Generic parameter that holds a string, an int or an pseudo-variable
 * @todo replace this with gparam_t
 */
struct multiparam_t {
	enum {
		MP_INT,
		MP_STR,
		MP_AVP,
		MP_PVE,
	} type;
	union {
		int n;
		str s;
		struct {
			unsigned short flags;
			int_str name;
		} a;
		pv_elem_t *p;
	} u;
};




/* ---- fixup functions: */
static int matrix_fixup(void** param, int param_no);

/* ---- exported commands: */
static int lookup_matrix(struct sip_msg *msg, struct multiparam_t *_first, struct multiparam_t *_second, struct multiparam_t *_dstavp);

/* ---- module init functions: */
static int mod_init(void);
static int child_init(int rank);
static int mi_child_init(void);
static void mod_destroy(void);

/* --- fifo functions */
struct mi_root * mi_reload_matrix(struct mi_root* cmd, void* param);  /* usage: kamctl fifo reload_matrix */




static cmd_export_t cmds[]={
	{ "matrix", (cmd_function)lookup_matrix, 3, matrix_fixup, 0, REQUEST_ROUTE | FAILURE_ROUTE },
	{ 0, 0, 0, 0, 0, 0}
};




static param_export_t params[] = {
	matrix_DB_URL
	matrix_DB_TABLE
	matrix_DB_COLS
	{ 0, 0, 0}
};




/* Exported MI functions */
static mi_export_t mi_cmds[] = {
	{ "reload_matrix", mi_reload_matrix, MI_NO_INPUT_FLAG, 0, mi_child_init },
	{ 0, 0, 0, 0, 0}
};




struct module_exports exports= {
	"matrix",
	DEFAULT_DLFLAGS,
	cmds,
	params,
	0,
	mi_cmds,
	0,
	0,
	mod_init,
	0,
	mod_destroy,
	child_init
};




struct first_t {
  struct first_t *next;
	int id;
	short int second_list[MAXCOLS+1];
};




struct matrix_t {
  struct first_t *head;
};




static gen_lock_t *lock = NULL;
static struct matrix_t *matrix = NULL;




/**
 * fixes the module functions' parameters if it is a phone number.
 * supports string, pseudo-variables and AVPs.
 *
 * @param param the parameter
 *
 * @return 0 on success, -1 on failure
 */
static int mp_fixup(void ** param) {
	pv_spec_t avp_spec;
	struct multiparam_t *mp;
	str s;

	mp = (struct multiparam_t *)pkg_malloc(sizeof(struct multiparam_t));
	if (mp == NULL) {
		LM_ERR("out of pkg memory\n");
		return -1;
	}
	memset(mp, 0, sizeof(struct multiparam_t));
	
	s.s = (char *)(*param);
	s.len = strlen(s.s);

	if (s.s[0]!='$') {
		/* This is string */
		mp->type=MP_STR;
		mp->u.s=s;
	}
	else {
		/* This is a pseudo-variable */
		if (pv_parse_spec(&s, &avp_spec)==0) {
			LM_ERR("pv_parse_spec failed for '%s'\n", (char *)(*param));
			pkg_free(mp);
			return -1;
		}
		if (avp_spec.type==PVT_AVP) {
			/* This is an AVP - could be an id or name */
			mp->type=MP_AVP;
			if(pv_get_avp_name(0, &(avp_spec.pvp), &(mp->u.a.name), &(mp->u.a.flags))!=0) {
				LM_ERR("Invalid AVP definition <%s>\n", (char *)(*param));
				pkg_free(mp);
				return -1;
			}
		} else {
			mp->type=MP_PVE;
			if(pv_parse_format(&s, &(mp->u.p))<0) {
				LM_ERR("pv_parse_format failed for '%s'\n", (char *)(*param));
				pkg_free(mp);
				return -1;
			}
		}
	}
	*param = (void*)mp;

	return 0;
}




/**
 * fixes the module functions' parameters in case of AVP names.
 *
 * @param param the parameter
 *
 * @return 0 on success, -1 on failure
 */
static int avp_name_fixup(void ** param) {
	pv_spec_t avp_spec;
	struct multiparam_t *mp;
	str s;

	s.s = (char *)(*param);
	s.len = strlen(s.s);
	if (s.len <= 0) return -1;
	if (pv_parse_spec(&s, &avp_spec)==0 || avp_spec.type!=PVT_AVP) {
		LM_ERR("Malformed or non AVP definition <%s>\n", (char *)(*param));
		return -1;
	}
	
	mp = (struct multiparam_t *)pkg_malloc(sizeof(struct multiparam_t));
	if (mp == NULL) {
		LM_ERR("out of pkg memory\n");
		return -1;
	}
	memset(mp, 0, sizeof(struct multiparam_t));
	
	mp->type=MP_AVP;
	if(pv_get_avp_name(0, &(avp_spec.pvp), &(mp->u.a.name), &(mp->u.a.flags))!=0) {
		LM_ERR("Invalid AVP definition <%s>\n", (char *)(*param));
		pkg_free(mp);
		return -1;
	}

	*param = (void*)mp;
	
	return 0;
}




static int matrix_fixup(void** param, int param_no)
{
	if (param_no == 1) {
		/* source id */
		if (mp_fixup(param) < 0) {
			LM_ERR("cannot fixup parameter %d\n", param_no);
			return -1;
		}
	}
	else if (param_no == 2) {
		/* destination id */
		if (mp_fixup(param) < 0) {
			LM_ERR("cannot fixup parameter %d\n", param_no);
			return -1;
		}
	}
	else if (param_no == 3) {
		/* destination avp name */
		if (avp_name_fixup(param) < 0) {
			LM_ERR("cannot fixup parameter %d\n", param_no);
			return -1;
		}
	}

	return 0;
}




static void matrix_clear(void)
{
	struct first_t *srcitem;
	if (matrix) {
		while (matrix->head) {
			srcitem = matrix->head;
			matrix->head = srcitem->next;
			shm_free(srcitem);
		}
	}
}




static int matrix_insert(int first, short int second, int res)
{
	struct first_t *srcitem;
	int i;

	if ((second<0) || (second>MAXCOLS)) {
		LM_ERR("invalid second value %d\n", second);
		return -1;
	}
	LM_DBG("searching for %d, %d\n", first, second);
	if (matrix) {
		srcitem = matrix->head;
		while (srcitem) {
			if (srcitem->id == first) {
				srcitem->second_list[second] = res;
				LM_DBG("inserted (%d, %d, %d)\n", first, second, res);
				return 0;
			}
			srcitem = srcitem->next;
		}
		/* not found */
		srcitem = shm_malloc(sizeof(struct first_t));
		if (srcitem == NULL) {
			LM_ERR("out of shared memory.\n");
			return -1;
		}
		memset(srcitem, 0, sizeof(struct first_t));

		/* Mark all new cells as empty */
		for (i=0; i<=MAXCOLS; i++) srcitem->second_list[i] = -1;

		srcitem->next = matrix->head;
		srcitem->id = first;
		srcitem->second_list[second] = res;
		matrix->head = srcitem;
	}

	LM_DBG("inserted new row for (%d, %d, %d)\n", first, second, res);
	return 0;
}




/* Returns the res id if the matrix contains an entry for the given indices, -1 otherwise.
 */
static int internal_lookup(int first, short int second)
{
	struct first_t *item;

	if ((second<0) || (second>MAXCOLS)) {
		LM_ERR("invalid second value %d\n", second);
		return -1;
	}

	if (matrix) {
		item = matrix->head;
		while (item) {
			if (item->id == first) {
				return item->second_list[second];
			}
			item = item->next;
		}
	}

	return -1;
}




static int lookup_matrix(struct sip_msg *msg, struct multiparam_t *_srctree, struct multiparam_t *_second, struct multiparam_t *_dstavp)
{
	int first;
	int second;
	struct usr_avp *avp;
	int_str avp_val;

	switch (_srctree->type) {
	case MP_INT:
		first = _srctree->u.n;
		break;
	case MP_AVP:
		avp = search_first_avp(_srctree->u.a.flags, _srctree->u.a.name, &avp_val, 0);
		if (!avp) {
			LM_ERR("cannot find srctree AVP\n");
			return -1;
		}
		if ((avp->flags&AVP_VAL_STR)) {
			LM_ERR("cannot process string value in srctree AVP\n");
			return -1;
		}
		else first = avp_val.n;
		break;
	default:
		LM_ERR("invalid srctree type\n");
		return -1;
	}

	switch (_second->type) {
	case MP_INT:
		second = _second->u.n;
		break;
	case MP_AVP:
		avp = search_first_avp(_second->u.a.flags, _second->u.a.name, &avp_val, 0);
		if (!avp) {
			LM_ERR("cannot find second_value AVP\n");
			return -1;
		}
		if ((avp->flags&AVP_VAL_STR)) {
			LM_ERR("cannot process string value in second_value AVP\n");
			return -1;
		}
		else second = avp_val.n;
		break;
	default:
		LM_ERR("invalid second_value type\n");
		return -1;
	}
	

	/* critical section start: avoids dirty reads when updating d-tree */
	lock_get(lock);

	avp_val.n=internal_lookup(first, second);

	/* critical section end */
	lock_release(lock);

	if (avp_val.n<0) {
		LM_INFO("lookup failed\n");
		return -1;
	}

	/* set avp ! */
	if (add_avp(_dstavp->u.a.flags, _dstavp->u.a.name, avp_val)<0) {
		LM_ERR("add AVP failed\n");
		return -1;
	}
	LM_INFO("result from lookup: %d\n", avp_val.n);
	return 1;
}




/**
 * Rebuild matrix using database entries
 * \return negative on failure, positive on success, indicating the number of matrix entries
 */
static int db_reload_matrix(void)
{
	db_key_t columns[3] = { &matrix_first_col, &matrix_second_col, &matrix_res_col };
	db1_res_t *res;
	int i;
	int n = 0;
	
	if (matrix_dbf.use_table(matrix_dbh, &matrix_table) < 0) {
		LM_ERR("cannot use table '%.*s'.\n", matrix_table.len, matrix_table.s);
		return -1;
	}
	if (matrix_dbf.query(matrix_dbh, NULL, NULL, NULL, columns, 0, 3, NULL, &res) < 0) {
		LM_ERR("error while executing query.\n");
		return -1;
	}

	/* critical section start: avoids dirty reads when updating d-tree */
	lock_get(lock);

	matrix_clear();

	if (RES_COL_N(res) > 2) {
		for(i = 0; i < RES_ROW_N(res); i++) {
			if ((!RES_ROWS(res)[i].values[0].nul) && (!RES_ROWS(res)[i].values[1].nul)) {
				if ((RES_ROWS(res)[i].values[0].type == DB1_INT) &&
						(RES_ROWS(res)[i].values[1].type == DB1_INT) &&
						(RES_ROWS(res)[i].values[2].type == DB1_INT)) {
					matrix_insert(RES_ROWS(res)[i].values[0].val.int_val, RES_ROWS(res)[i].values[1].val.int_val, RES_ROWS(res)[i].values[2].val.int_val);
					n++;
				}
				else {
					LM_ERR("got invalid result type from query.\n");
				}
			}
		}
	}

	/* critical section end */
	lock_release(lock);

	matrix_dbf.free_result(matrix_dbh, res);

	LM_INFO("loaded %d matrix entries.\n", n);
	return n;
}




static int init_shmlock(void)
{
	lock = lock_alloc();
	if (!lock) {
		LM_CRIT("cannot allocate memory for lock.\n");
		return -1;
	}
	if (lock_init(lock) == 0) {
		LM_CRIT("cannot initialize lock.\n");
		return -1;
	}

	return 0;
}




static void destroy_shmlock(void)
{
	if (lock) {
		lock_destroy(lock);
		lock_dealloc((void *)lock);
		lock = NULL;
	}
}




struct mi_root * mi_reload_matrix(struct mi_root* cmd, void* param)
{
	struct mi_root * tmp = NULL;
	if(db_reload_matrix() >= 0) {
		tmp = init_mi_tree( 200, MI_OK_S, MI_OK_LEN);
	} else {
		tmp = init_mi_tree( 500, "cannot reload matrix", 24);
	}

	return tmp;
}




static int init_matrix(void)
{
	matrix = shm_malloc(sizeof(struct matrix_t));
	if (!matrix) {
		LM_ERR("out of shared memory\n");
		return -1;
	}
	memset(matrix, 0, sizeof(struct matrix_t));
	if (db_reload_matrix() < 0) {
		LM_ERR("cannot populate matrix\n");
		return -1;
	}

	return 0;
}




static void destroy_matrix(void)
{
	if (matrix) {
		matrix_clear();
		shm_free(matrix);
	}
}




static int mod_init(void)
{
	if(register_mi_mod(exports.name, mi_cmds)!=0)
	{
		LM_ERR("failed to register MI commands\n");
		return -1;
	}

	if (init_shmlock() != 0) return -1;
	if (matrix_db_init() != 0) return -1;
	if (matrix_db_open() != 0) return -1;
	if (init_matrix() != 0) return -1;
	matrix_db_close();
	return 0;
}




static int child_init(int rank)
{
	if(rank==PROC_INIT || rank==PROC_TCP_MAIN)
		return 0;
	if (matrix_db_open() != 0) return -1;
	return 0;
}




static int mi_child_init(void)
{
	if (matrix_db_open() != 0) return -1;
	return 0;
}




static void mod_destroy(void)
{
	destroy_matrix();
	destroy_shmlock();
	matrix_db_close();
}
