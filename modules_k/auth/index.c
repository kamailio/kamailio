/*
 * $Id:$
 *
 * Nonce index  related functions
 *
 * Copyright (C)2008  Voice System S.R.L
 *
 * This file is part of Kamailio, a free SIP server.
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
 * Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307  USA
 *
 * History:
 * --------
 *  2008-05-29  initial version (anca)
 */

#include <stdio.h>
#include "../../dprint.h"
#include "../../timer.h"
#include "index.h"
#include "auth_mod.h"

#define set_buf_bit(index)    \
    do{\
        nonce_buf[index>>3] |=  (1<<(index%8));\
    }while(0)

#define unset_buf_bit(index)    \
    do{\
        nonce_buf[index>>3] &=  ~(1<<(index%8));\
    }while(0)

#define check_buf_bit(index)  ( nonce_buf[index>>3] & (1<<(index%8)) )

/*
 *  Get a valid index for the new nonce
 */

int reserve_nonce_index(void)
{
    unsigned int curr_sec;
    int index;
    int i;


    curr_sec =  get_ticks()%(nonce_expire+1);

    lock_get(nonce_lock);

    /* update last index for the previous seconds */
    if(*next_index== -1) /* for the first request */
        *next_index= 0;
    else
    {
        if(*second!= curr_sec)
        {
            index= (*next_index==NBUF_LEN)?NBUF_LEN-1:*next_index -1;

            if(curr_sec> *second)
            {
                for (i= *second; i< curr_sec; i++)
                    sec_monit[i]= index;
            }
            else
            {
                for (i= *second; i<= nonce_expire; i++)
                    sec_monit[i]= index;

                for (i= 0; i< curr_sec; i++)
                    sec_monit[i]= index;
            }
        }
    }
    *second= curr_sec;

    if(sec_monit[curr_sec]== -1) /* if in the first second*/
    {
        if(*next_index == NBUF_LEN)
        {
            lock_release(nonce_lock);
            return -1;
        }


        goto done;
    }

    if(*next_index> sec_monit[curr_sec]) /* if at the end of the buffer */
    {
        /* if at the end of the buffer */
        if(*next_index == NBUF_LEN)
        {
            *next_index = 0;
            goto index_smaller;
        }
        goto done;
    }

index_smaller:
    if(*next_index== sec_monit[curr_sec])  /* no more space -> return error */
    {
        lock_release(nonce_lock);
        LM_INFO("no more indexes available\n");
        return -1;
    }

done:
    unset_buf_bit(*next_index);
    index= *next_index;
    *next_index = *next_index + 1;
    LM_DBG("second= %d, sec_monit= %d,  index= %d\n", *second, sec_monit[curr_sec], index);
    lock_release(nonce_lock);
    return index;
}

/*
 *  Check if the nonce has been used before
 */


int is_nonce_index_valid(int index)
{
    /* if greater than NBUF_LEN ->error */
    
    if(index>= NBUF_LEN )
    {
        LM_ERR("index greater than buffer length\n");
        return 0;
    }

    lock_get(nonce_lock);

    /* if in the first 30 seconds */
    if(sec_monit[*second]== -1)
    {
        if(index>= *next_index)
        {
            LM_DBG("index out of range\n");
            lock_release(nonce_lock);
            return 0;
        }
        else
        {
            set_buf_bit(index);
            lock_release(nonce_lock);
            return 1;
        }
    }

    /* check if right interval */
    if(*next_index < sec_monit[*second])
    {
        if(!(index>= sec_monit[*second] || index<= *next_index))
        {
            LM_DBG("index out of the permitted interval\n");
            goto error;
        }
    }
    else
    {
        if(!(index >= sec_monit[*second] && index<=*next_index))
        {
            LM_DBG("index out of the permitted interval\n");
            goto error;
        }
    }

    /* check if the first time used */
    if(check_buf_bit(index))
    {
        LM_DBG("nonce already used\n");
        goto error;
    }
    
    set_buf_bit(index);
    lock_release(nonce_lock);
    return 1;

error:
    lock_release(nonce_lock);
    return 0;

}

