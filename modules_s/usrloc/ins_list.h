/* 
 * $Id$
 *
 */


#ifndef INS_LIST_H
#define INS_LIST_H

#include "ucontact.h"
#include "../../str.h"


struct ins_itm {
	struct ins_itm* next;
	time_t expires;
	float q;
	int cseq;
	str* user;
	str* cont;
	int cid_len;
	char callid[0];
};


int put_on_ins_list(struct ucontact* _c);

int process_ins_list(str* _d);


#endif /* INS_LIST_H */
