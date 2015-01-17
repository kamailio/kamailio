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

#ifndef MOHQ_DB_H
#define MOHQ_DB_H

/**********
* DB definitions
**********/

/* table versions */
#define MOHQ_CTABLE_VERSION  1
#define MOHQ_QTABLE_VERSION  1

/* mohqueues columns */
#define MOHQ_COLCNT   6
#define MOHQCOL_ID    0
#define MOHQCOL_URI   1
#define MOHQCOL_MDIR  2
#define MOHQCOL_MFILE 3
#define MOHQCOL_NAME  4
#define MOHQCOL_DEBUG 5

/* mohqcalls columns */
#define CALL_COLCNT   6
#define CALLCOL_STATE 0
#define CALLCOL_CALL  1
#define CALLCOL_MOHQ  2
#define CALLCOL_FROM  3
#define CALLCOL_CNTCT 4
#define CALLCOL_TIME  5

/**********
* DB function declarations
**********/

void add_call_rec (int);
void clear_calls (db1_con_t *);
void delete_call_rec (call_lst *);
db1_con_t *mohq_dbconnect (void);
void mohq_dbdisconnect (db1_con_t *);
void update_call_rec (call_lst *);
void update_debug (mohq_lst *, int);
void update_mohq_lst (db1_con_t *pconn);

#endif /* MOHQ_DB_H */
