/*
 * $Id$
 *
 * interface for modules
 */

#ifndef mod_iface_h
#define mod_iface_h


struct hdr_lst{
	int type; /* VIA, OTHER, UNSPEC(=0), ... */
	int op;   /* DEL, ADD, NOP, UNSPEC(=0) */
	
	union{
		int offset; /* used for DEL, MODIFY */
		char * value; /* used for ADD */
	}u;
	int len; /* length of this header field */
	
	
	struct hdr_lst* before; /* list of headers to be inserted in front of the
								current one */
	struct hdr_lst* after; /* list of headers to be inserted immediately after
							  the current one */
	
	struct hdr_lst* next;
};

/*
 * hdrs must be kept sorted after their offset (DEL, NOP, UNSPEC)
 * and/or their position (ADD). E.g.:
 *  - to delete header Z insert it in to the list according to its offset 
 *   and with op=DELETE
 * - if you want to add a new header X after a  header Y, insert Y in the list
 *   with op NOP and after it X (op ADD).
 * - if you want X before Y, insert X in Y's before list.
 * - if you want X to be the first header just put it first in hdr_lst.
 *  -if you want to replace Y with X, insert Y with op=DELETE and then X with
 *  op=ADD.
 * before and after must contain only ADD ops!
 * 
 * Difference between "after" & "next" when ADDing:
 * "after" forces the new header immediately after the current one while
 * "next" means another header can be inserted between them.
 * 
 */


#endif
