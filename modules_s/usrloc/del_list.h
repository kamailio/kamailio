/* 
 * $Id$
 *
 */

#ifndef DEL_LIST_H
#define DEL_LIST_H


#include "ucontact.h"


struct del_itm {
	struct del_itm* next;
	int user_len;
	int cont_len;
	char tail[0];
};


int put_on_del_list(struct ucontact* _c);

int process_del_list(str* _d);


#endif /* DEL_LIST_H */
