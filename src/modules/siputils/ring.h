/*
 * Copyright (C) 2008-2009 1&1 Internet AG
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
 * Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA  02110-1301  USA
 *
 */

/**
 * \file
 * \brief SIP-utils :: Module with several utiltity functions related to SIP messages handling
 * \ingroup siputils
 * - Module; \ref siputils
 */

#ifndef RING_H
#define RING_H


#define HASHTABLEBITS 13
#define HASHTABLESIZE (((unsigned int)1) << HASHTABLEBITS)
#define HASHTABLEMASK (HASHTABLESIZE - 1)
#define MAXCALLIDLEN 255


extern gen_lock_t *ring_lock;


/*!
 * \brief  Inserts callid of message into hashtable
 *
 * Inserts callid of message into hashtable. Any 183 messages with
 * this callid that occur in the next ring_timeout seconds, will be
 * converted to 180.
 * \param msg SIP message
 * \param unused1 unused
 * \param unused2 unused
 * \return 1 on success, -1 otherwise
 */
int ring_insert_callid(struct sip_msg *msg, char *unused1, char *unused2);


/*!
 * \brief Initialize the ring hashtable in shared memory
 */
void ring_init_hashtable(void);


/*!
 * \brief Destroy the ring hashtable
 */
void ring_destroy_hashtable(void);


/*!
 * \brief Callback function that does the work inside the server.
 * \param msg SIP message
 * \param flags unused
 * \param bar unused
 * \return 1 on success, -1 on failure
 */
int ring_filter(struct sip_msg *msg, unsigned int flags, void *bar);


/*!
 * \brief Fixup function for the ring_insert_callid function
 * \param param unused
 * \param param_no unused
 * \return 0
 */
int ring_fixup(void ** param, int param_no);


#endif
