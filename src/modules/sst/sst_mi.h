/*
 * Copyright (C) 2008 SOMA Networks, Inc.
 * Written By Ovidiu Sas (osas)
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

/*!
 *\file sst/sst_mi.h
 *\brief Functions for the SST module
 * \ref sst/sst_mi.c
 * \ingroup sst
 * Module: \ref sst
 */


#ifndef _SST_MI_H_
#define _SST_MI_H_

#include "../dialog/dlg_load.h"

/**
 * The dialog mi helper function.
 */
void sst_dialog_mi_context_CB(struct dlg_cell* did, int type, struct dlg_cb_params * params);

#endif /* _SST_MI_H_ */
