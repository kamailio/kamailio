/* 
 * Copyright (C) 2005 iptelorg GmbH
 *
 * This file is part of ser, a free SIP server.
 *
 * ser is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version
 *
 * For a license to use the ser software under conditions
 * other than those described here, or to purchase support for this
 * software, please contact iptel.org by e-mail at the following addresses:
 *    info@iptel.org
 *
 * ser is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA  02110-1301  USA
 */

#ifndef __DLG_MOD_INTERNAL_H
#define __DLG_MOD_INTERNAL_H

#include "../../modules/tm/tm_load.h"
#include "dlg_mod.h"

extern struct tm_binds tmb;

/* data members for pregenerated tags - taken from TM */
extern char *dialog_tags;
extern char *dialog_tag_suffix;

extern gen_lock_t *dlg_mutex;
#define lock_dialog_mod()	lock_get(dlg_mutex)
#define unlock_dialog_mod()	lock_release(dlg_mutex)

#endif
