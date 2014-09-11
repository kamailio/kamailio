/*
 * $Id$
 *
 * Copyright (C) 2012 Smile Communications, jason.penton@smilecoms.com
 * Copyright (C) 2012 Smile Communications, richard.good@smilecoms.com
 * 
 * The initial version of this code was written by Dragos Vingarzan
 * (dragos(dot)vingarzan(at)fokus(dot)fraunhofer(dot)de and the
 * Fruanhofer Institute. It was and still is maintained in a separate
 * branch of the original SER. We are therefore migrating it to
 * Kamailio/SR and look forward to maintaining it from here on out.
 * 2011/2012 Smile Communications, Pty. Ltd.
 * ported/maintained/improved by 
 * Jason Penton (jason(dot)penton(at)smilecoms.com and
 * Richard Good (richard(dot)good(at)smilecoms.com) as part of an 
 * effort to add full IMS support to Kamailio/SR using a new and
 * improved architecture
 * 
 * NB: Alot of this code was originally part of OpenIMSCore,
 * FhG Fokus. 
 * Copyright (C) 2004-2006 FhG Fokus
 * Thanks for great work! This is an effort to 
 * break apart the various CSCF functions into logically separate
 * components. We hope this will drive wider use. We also feel
 * that in this way the architecture is more complete and thereby easier
 * to manage in the Kamailio/SR environment
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

#ifndef __RECEIVER_H
#define __RECEIVER_H

#include "peer.h"
#include "diameter.h"

/** Maximum incoming message length */
#define DP_MAX_MSG_LENGTH 65536

#define DIAMETER_HEADER_LEN 20


typedef enum {
	Receiver_Waiting=0,
	Receiver_Header=1,
	Receiver_Rest_of_Message=2
} receiver_state_t;

/** list of receiver attached peers */
typedef struct _serviced_peer_t {
	peer *p;									/**< the attached peer */
	
	int tcp_socket;								/**< socket used for the Diameter communication */
	
	str send_pipe_name;							/**< name of the pipe to signal messages to be sent out */ 
	int send_pipe_fd;							/**< reader from the pipe to signal messages to be sent out */
	int send_pipe_fd_out;						/**< keep-alive writer for the pipe to signal messages to be sent out */

	receiver_state_t state;						/**< current receiving state */
	char buf[DIAMETER_HEADER_LEN];				/**< buffer to receive header into */
	int buf_len;								/**< received bytes in the header */
	int length;									/**< length of the message as written in the header */
	char *msg;									/**< dynamic buffer for receiving one message */
	int msg_len;								/**< received bytes in the dynamic buffer */
		
	
	struct _serviced_peer_t *next;	/**< first peer in the list */	
	struct _serviced_peer_t *prev; /**< last peer in the list */
} serviced_peer_t;

int receiver_init(peer *p);
void receiver_process(peer *p);

int receiver_send_socket(int sock,peer *p);

int peer_connect(peer *p);
int peer_send(peer *p,int sock,AAAMessage *msg,int locked);
int peer_send_msg(peer *p,AAAMessage *msg);

#endif

