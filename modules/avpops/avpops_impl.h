/*
 * Copyright (C) 2004-2006 Voice Sistem SRL
 *
 * This file is part of Kamailio.
 *
 * Kamailio is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License
 * as published by the Free Software Foundation; either version 2
 * of the License, or (at your option) any later version.
 *
 * Kamailio is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA  02110-1301, USA.
 *
 */



#ifndef _AVP_OPS_IMPL_H_
#define _AVP_OPS_IMPL_H_

#include "../../str.h"
#include "../../usr_avp.h"
#include "../../pvar.h"
#include "../../re.h"
#include "../../parser/msg_parser.h"

#include "avpops_db.h"



/* flags used inside avps */
/* IMPORTANT: flagss 0-4 are reserved by core; 8-15 by script */
#define AVP_IS_IN_DB    (1<<12)

/* DB flags */
#define AVPOPS_DB_NAME_INT   (1<<1)
#define AVPOPS_DB_VAL_INT    (1<<0)

/* operand flags */
#define AVPOPS_VAL_NONE      (1<<0)
#define AVPOPS_VAL_INT       (1<<1)
#define AVPOPS_VAL_STR       (1<<2)
#define AVPOPS_VAL_PVAR      (1<<3)

#define AVPOPS_USE_FROM      (1<<5)
#define AVPOPS_USE_TO        (1<<6)
#define AVPOPS_USE_RURI      (1<<7)
#define AVPOPS_USE_USERNAME  (1<<8)
#define AVPOPS_USE_DOMAIN    (1<<9)

#define AVPOPS_USE_SRC_IP    (1<<12)
#define AVPOPS_USE_DST_IP    (1<<13)
#define AVPOPS_USE_DURI      (1<<14)
#define AVPOPS_USE_BRANCH    (1<<15)

/* flags for operation flags    24..31 */
#define AVPOPS_FLAG_USER0    (1<<24)
#define AVPOPS_FLAG_DOMAIN0  (1<<25)
#define AVPOPS_FLAG_URI0     (1<<26)
#define AVPOPS_FLAG_UUID0    (1<<27)

/* operation flags  */
#define AVPOPS_OP_EQ        (1<<0)
#define AVPOPS_OP_NE        (1<<1)
#define AVPOPS_OP_LT        (1<<2)
#define AVPOPS_OP_LE        (1<<3)
#define AVPOPS_OP_GT        (1<<4)
#define AVPOPS_OP_GE        (1<<5)
#define AVPOPS_OP_RE        (1<<6)
#define AVPOPS_OP_FM        (1<<7)
#define AVPOPS_OP_BAND      (1<<8)
#define AVPOPS_OP_BOR       (1<<9)
#define AVPOPS_OP_BXOR      (1<<10)
#define AVPOPS_OP_BNOT      (1<<11)
#define AVPOPS_OP_ADD       (1<<12)
#define AVPOPS_OP_SUB       (1<<13)
#define AVPOPS_OP_MUL       (1<<14)
#define AVPOPS_OP_DIV       (1<<15)
#define AVPOPS_OP_MOD       (1<<16)

/* flags for operation flags    24..31 */
#define AVPOPS_FLAG_ALL     (1<<24)
#define AVPOPS_FLAG_CI      (1<<25)
#define AVPOPS_FLAG_DELETE  (1<<26)
#define AVPOPS_FLAG_CASTN   (1<<27)
#define AVPOPS_FLAG_CASTS   (1<<28)
#define AVPOPS_FLAG_EMPTY   (1<<29)

/* container structer for Flag+Int_Spec_value parameter */
struct fis_param
{
	int     ops;       /* operation flags */
	int     opd;       /* operand flags */
	int     type;
	union {
		pv_spec_t *sval;    /* values int or str */
		int n;
		str s;
	} u;
};

struct db_param
{
	struct fis_param a;        /* attribute */
	str              sa;       /* attribute as str (for db queries) */
	str              table;    /* DB table/scheme name */
	struct db_scheme *scheme;  /* DB scheme */
};

void init_store_avps(str **db_columns);

int ops_dbload_avps (struct sip_msg* msg, struct fis_param *sp,
								struct db_param *dbp, int use_domain);

int ops_dbdelete_avps(struct sip_msg* msg, struct fis_param *sp,
								struct db_param *dbp, int use_domain);

int ops_dbstore_avps(struct sip_msg* msg, struct fis_param *sp,
								struct db_param *dbp, int use_domain);

int ops_dbquery_avps(struct sip_msg* msg, pv_elem_t* query,
								pvname_list_t* dest);

int ops_delete_avp(struct sip_msg* msg,
								struct fis_param *ap);

int ops_copy_avp(struct sip_msg* msg, struct fis_param* name1,
								struct fis_param* name2);

int ops_pushto_avp(struct sip_msg* msg, struct fis_param* dst,
								struct fis_param* ap);

int ops_check_avp(struct sip_msg* msg, struct fis_param* param,
								struct fis_param* check);

int ops_op_avp(struct sip_msg* msg, struct fis_param** param,
								struct fis_param* op);

int ops_subst(struct sip_msg* msg, struct fis_param** src,
		struct subst_expr* subst);

int ops_is_avp_set(struct sip_msg* msg, struct fis_param *ap);

int ops_print_avp(void);

#endif

