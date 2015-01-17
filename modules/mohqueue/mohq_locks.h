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

#ifndef MOHQ_LOCKS_H
#define MOHQ_LOCKS_H

#include "../../locking.h"

/**********
* lock record
**********/

typedef struct
  {
  gen_lock_t *plock;
  int lock_cnt;
  } mohq_lock;

/**********
* function declarations
**********/

int mohq_lock_change (mohq_lock *, int);
void mohq_lock_destroy (mohq_lock *);
int mohq_lock_init (mohq_lock *);
void mohq_lock_release (mohq_lock *);
int mohq_lock_set (mohq_lock *, int, int);

#endif /* MOHQ_LOCKS_H */
