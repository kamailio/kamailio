/*
 * Copyright (C) 2013 Robert Boisvert
 *
 * This file is part of the mohqueue module for Kamailio, a free SIP server.
 *
 * The mohqueue module is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version
 *
 * The mohqueue module is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA  02110-1301  USA
 *
 */

#ifndef MOHQ_FUNCS_H
#define MOHQ_FUNCS_H

/**********
* module function declarations
**********/

rtpmap **find_MOH (char *, char *);
struct mi_root *mi_debug (struct mi_root *, void *);
struct mi_root *mi_drop_call (struct mi_root *, void *);
int mohq_count (sip_msg_t *, char *, pv_spec_t *);
void mohq_debug (mohq_lst *, char *, ...);
int mohq_process (sip_msg_t *);
int mohq_retrieve (sip_msg_t *, char *, char *);
int mohq_send (sip_msg_t *, char *);

#endif /* MOHQ_FUNCS_H */
