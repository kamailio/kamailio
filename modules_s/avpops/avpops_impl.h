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



#ifndef _AVP_OPS_IMPL_H_
#define _AVP_OPS_IMPL_H_

#include "../../str.h"
#include "../../usr_avp.h"
#include "../../parser/msg_parser.h"



/* flags used inside avps */
/* IMPORTANT: falgs 0,1 are reserved by avp core */
#define AVP_IS_IN_DB    (1<<3)

/* DB flags */
#define AVPOPS_DB_NAME_INT   (1<<1)
#define AVPOPS_DB_VAL_INT    (1<<0)

/* flags about the value (used by fis structure) 0..15  */
#define AVPOPS_VAL_NONE      (1<<0)
#define AVPOPS_VAL_INT       (1<<1)
#define AVPOPS_VAL_STR       (1<<2)
#define AVPOPS_VAL_AVP       (1<<3)

#define AVPOPS_USE_FROM      (1<<5)
#define AVPOPS_USE_TO        (1<<6)
#define AVPOPS_USE_RURI      (1<<7)
#define AVPOPS_USE_USERNAME  (1<<8)
#define AVPOPS_USE_DOMAIN    (1<<9)
#define AVPOPS_USE_HDRREQ    (1<<10)
#define AVPOPS_USE_HDRRPL    (1<<11)
#define AVPOPS_USE_SRC_IP    (1<<12)

/* flags about operation  16..23  */
#define AVPOPS_OP_EQ        (1<<16)
#define AVPOPS_OP_LT        (1<<17)
#define AVPOPS_OP_GT        (1<<18)

/* flags for flags    24..31 */
#define AVPOPS_FLAG_ALL     (1<<24)
#define AVPOPS_FLAG_CI      (1<<25)
#define AVPOPS_FLAG_USER    (1<<26)
#define AVPOPS_FLAG_DOMAIN  (1<<27)


/* container structer for Flag+Int_Str_value parameter */
struct fis_param
{
	int     flags;  /* flags */
	int_str val;    /* values int or str */
};

struct db_param
{
	struct fis_param a;     /* attribute */
	str              sa;     /* attribute as str (for db queries) */
	str              table;  /* table name */
};


void init_store_avps( char **db_columns);

int ops_dbload_avps (struct sip_msg* msg, struct fis_param *sp,
								struct db_param *dbp, int use_domain);

int ops_dbstore_avps( struct sip_msg* msg, struct fis_param *sp,
								struct db_param *dbp, int use_domain);

int ops_dbdelete_avps( struct sip_msg* msg, struct fis_param *sp,
								struct db_param *dbp, int use_domain);

int ops_write_avp( struct sip_msg* msg, struct fis_param *src,
								struct fis_param *ap);

int ops_delete_avp( struct sip_msg* msg,
								struct fis_param *ap);

int ops_pushto_avp( struct sip_msg* msg, struct fis_param* dst,
								struct fis_param* ap);

int ops_check_avp( struct sip_msg* msg, struct fis_param* param,
								struct fis_param* check);

int ops_print_avp();

#endif

