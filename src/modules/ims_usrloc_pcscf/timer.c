/*
 * Copyright (C) 2026 toharishs@gmail.com
 *
 * The initial version of this code is written by Harish S
 *
 * SPDX-License-Identifier: GPL-2.0-or-later
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

/* Periodic cleanup timer for expired temp-GRUU history rows */

#include "../../core/dprint.h"

#include "ims_usrloc_pcscf_mod.h"
#include "usrloc.h"

extern int db_mode;
extern int db_cleanup_temp_gruu_history(void);

void ul_timer_cleanup_temp_gruu_history(void)
{
	if(db_mode == NO_DB)
		return;

	if(db_cleanup_temp_gruu_history() < 0) {
		LM_WARN("temp GRUU history cleanup failed\n");
	}
}
