/*
 * $Id$
 *
 * Copyright (C) 2001-2003 Fhg Fokus
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
 * Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307  USA
 */


#ifndef _HOOKS_H
#define _HOOKS_H

struct sip_msg;
struct cell;

typedef enum { TMCB_REPLY,  TMCB_E2EACK, TMCB_REPLY_IN, 
	TMCB_REQUEST_OUT, TMCB_LOCAL_COMPLETED, TMCB_ON_NEGATIVE,
	TMCB_END } tmcb_type;

/* 
	TMCB_REPLY	-  a reply has been sent out
	  no chance to change anything in the message; 
	  still good enough for many uses, such as accounting
	  of completed transactions; note well that the message
	  passed to the callback may also have value FAKED_REPLY,
	  i.e., refering to it will segfault
	TMCB_REPLY_IN - a reply was received and is about to be forwarded;
	  compared to TMCB_REPLY, it is a very internal callback and
	  you should use it with lot of caution
	  - it allows you to change the message (called before printing
	    the relayed message)
	  - it is called from a reply lock -- it is mroe dangerous and
	    anything you do makes the processes spend more time in
	    the lock, decreasing overall performance
	  - is is called only for replies >100, <300 (final replies
	    might be cached on forking, stored in shmem -- then, there
		is no more easy way to change messages)
	  - as it is called before printing and forwarding, there is
	    no guarantee the message will be sent out -- either can
	    fail

		Note: none of the reply callbacks will be evoked if
		"silent C timer" hits. Silent C timer is a feature which
		prevents cancellation of a call in progress by a server
		in the middle, when C timer expires. On one side, 
		INVITE transactional state cannot be kept for ever,
		on the other side you want to allow long ringing 
		uninterrupted by a proxy server. The silent_c feature
		-- if circumstances allow -- simply discards transaction
		state when C timer hits, the transaction can then complete
		statelessly. Then, however, the stateful callback will
		NOT be called. If you do not wish this behaviour (e.g.,
		for sake of transaction accounting, in which you do
		not desire users to wait until silent C hits and
		eventually complete an unaccounted transaction), turn
		silent C off either globaly (TM option "noisy_ctimer"
		set to 1) or for a specific transaction (you can for
		example set the transaction member "noisy_timer"
		from request callback.)

	TMCB_E2EACK - presumably, an end2end ACK was received and
		is about to be processed statelessly; you better don't
	    use this callback as there is no reliable way to match
	    an e2e ACK to an INVITE transaction, we just try it for
	    those, who believe they can't live without knowing about
	    the ACK; There are more reasons why the e2e ACK callback
	    is never triggered: 1) the e2eACK does not pass the server
	    at all 2) the e2e ACK does not match an INVITE transaction
		because its r-uri or via is different
	TMCB_REQUEST_OUT - a request was received and is about to be fwd-ed;
		it is not called on retransmissions; it is called prior to
		printing the relayed message, i.e., changes to it can
		be done
	TMCB_LOCAL_COMPLETED - a local transaction completed; note that
	    the callback parameter may be FAKED_REPLY
	TMCB_MISSED -- transaction was replied with a negative value;
		called from within a REPLY_LOCK, message may be FAKED_REPLY
	TMCB_ON_NEGATIVE -- called whenever a transaction is about to complete
	    with a negative result; it's a great time to introduce a new
	    uac (serial forking) or change the reply code; be cautions
	    though -- it is called from within REPLY_LOCK and careless
	    usage of the callback can easily result in a deadlock; msg
	    is always 0 (callback refers to whole transaction and not
	    to individual message), code is the currently lowest status
	    code
	TMCB_END	- just a bumper

	see the 'acc' module for an example of callback usage

	note that callbacks MUST be installed before forking
    (callback lists do not live in shmem and have no access
	protection)
*/

typedef void (transaction_cb) ( struct cell* t, struct sip_msg* msg, 
	int code, void *param );

struct tm_callback_s {
	int id;
	transaction_cb* callback;
	struct tm_callback_s* next;
	void *param;
};


extern struct tm_callback_s* callback_array[ TMCB_END ];

typedef int (*register_tmcb_f)(tmcb_type cbt, transaction_cb f, void *param);

int register_tmcb( tmcb_type cbt, transaction_cb f, void *param );
void callback_event( tmcb_type cbt, struct cell *trans,
	struct sip_msg *msg, int code );

#endif
