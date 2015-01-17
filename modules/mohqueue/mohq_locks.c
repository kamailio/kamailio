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

#include "mohq.h"
#include "mohq_locks.h"

/**********
* external functions
**********/

/**********
* Change Lock
*
* INPUT:
*   Arg (1) = lock pointer
*   Arg (2) = exclusive flag
* OUTPUT: 0 if failed
**********/

int mohq_lock_change (mohq_lock *plock, int bexcl)

{
/**********
* o lock memory
* o check set type
* o unlock memory
**********/

int nret = 0;
lock_get (plock->plock);
if (bexcl)
  {
  if (plock->lock_cnt == 1)
    {
    plock->lock_cnt = -1;
    nret = 1;
    }
  }
else
  {
  if (plock->lock_cnt == -1)
    {
    plock->lock_cnt = 1;
    nret = 1;
    }
  }
lock_release (plock->plock);
return nret;
}

/**********
* Destroy Lock Record
*
* INPUT:
*   Arg (1) = lock pointer
* OUTPUT: none
**********/

void mohq_lock_destroy (mohq_lock *plock)

{
lock_destroy (plock->plock);
lock_dealloc (plock->plock);
return;
}

/**********
* Init Lock Record
*
* INPUT:
*   Arg (1) = lock pointer
* OUTPUT: 0 if failed
**********/

int mohq_lock_init (mohq_lock *plock)

{
/**********
* alloc memory and initialize
**********/

char *pfncname = "mohq_lock_init: ";
plock->plock = lock_alloc ();
if (!plock->plock)
  {
  LM_ERR ("%sUnable to allocate lock memory!", pfncname);
  return 0;
  }
if (!lock_init (plock->plock))
  {
  LM_ERR ("%sUnable to init lock!", pfncname);
  lock_dealloc (plock->plock);
  return 0;
  }
plock->lock_cnt = 0;
return -1;
}

/**********
* Release Lock
*
* INPUT:
*   Arg (1) = lock pointer
* OUTPUT: none
**********/

void mohq_lock_release (mohq_lock *plock)

{
/**********
* o lock memory
* o reduce count
* o unlock memory
**********/

lock_get (plock->plock);
switch (plock->lock_cnt)
  {
  case -1:
    plock->lock_cnt = 0;
    break;
  case 0:
    LM_WARN ("mohq_lock_release: Lock was not set");
    break;
  default:
    plock->lock_cnt--;
    break;
  }
lock_release (plock->plock);
return;
}

/**********
* Set Lock
*
* INPUT:
*   Arg (1) = lock pointer
*   Arg (2) = exclusive flag
*   Arg (3) = milliseconds to try; 0 = try once
* OUTPUT: 0 if failed
**********/

int mohq_lock_set (mohq_lock *plock, int bexcl, int nms_cnt)

{
int nret = 0;
do
  {
  /**********
  * o lock memory
  * o check set type
  * o unlock memory
  * o sleep if failed
  **********/

  lock_get (plock->plock);
  if (bexcl)
    {
    if (!plock->lock_cnt)
      {
      plock->lock_cnt = -1;
      nret = 1;
      }
    }
  else
    {
    if (plock->lock_cnt != -1)
      {
      plock->lock_cnt++;
      nret = 1;
      }
    }
  lock_release (plock->plock);
  if (!nret)
    { usleep (1); }
  }
while (!nret && --nms_cnt >= 0);
return nret;
}
