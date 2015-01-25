/*
 * Copyright (C) 2006 SOMA Networks, Inc.
 * Written By Ron Winacott (karwin)
 *
 * This file is part of Kamailio, a free SIP server.
 *
 * Kamailio is free software; you can redistribute it and/or modify it
 * under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version
 *
 * Kamailio is distributed in the hope that it will be useful, but
 * WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA 02110-1301
 * USA
 *
 */

/*! \file sst/sst_handlers.h
 * \brief Session timer handling
 * \ingroup sst
 * Module: \ref sst
 */
 

#ifndef _SST_HANDLERS_H_
#define _SST_HANDLERS_H_

#include "../../pvar.h"
#include "../../parser/msg_parser.h"
#include "../dialog/dlg_load.h"


/*! \brief
 * Fag values used in the sst_info_t See below.
 */
enum sst_flags {
	SST_UNDF=0,             /* 0 - --- */
	SST_UAC=1,              /* 1 - 2^0 */
	SST_UAS=2,              /* 2 - 2^1 */
	SST_PXY=4,              /* 4 - 2^2 */
	SST_NSUP=8              /* 8 - 2^3 */
};

/** \brief
 * The local state required to figure out if and who supports SST and
 * if and who will be the refresher.
 */
typedef struct sst_info_st {
	enum sst_flags requester;
	enum sst_flags supported;
	unsigned int interval;
} sst_info_t;


/** \brief
 * The static (opening) callback function for all dialog creations
 */
void sst_dialog_created_CB(struct dlg_cell *did, int type, 
		struct dlg_cb_params * params);

/** \brief
 * The script function
 */
int sst_check_min(struct sip_msg *msg, char *str1, char *str2);

/** \brief
 * The handlers initializer function
 */
void sst_handler_init(pv_spec_t *timeout_avp, unsigned int minSE, 
		int flag, unsigned int reject);

#endif /* _SST_HANDLERS_H_ */
