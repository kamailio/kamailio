/* $Id$
 *
 */

#include "data_lump.h"
#include "dprint.h"

#include <stdlib.h>

#ifdef DEBUG_DMALLOC
#include <dmalloc.h>
#endif



/* adds a header to the end
 * returns  pointer on success, 0 on error */
struct lump* append_new_lump(struct lump** list, char* new_hdr,
							 int len, int type)
{
	struct lump** t;
	struct lump* tmp;
	
	for (t=list;*t;t=&((*t)->next));

	tmp=malloc(sizeof(struct lump));
	if (tmp==0){
		LOG(L_ERR, "ERROR: append_new_lump: out of memory\n");
		return 0;
	}
		
	memset(tmp,0,sizeof(struct lump));
	tmp->type=type;
	tmp->op=LUMP_ADD;
	tmp->u.value=new_hdr;
	tmp->len=len;
	*t=tmp;
	return tmp;
}



/* inserts a header to the beginning 
 * returns pointer if success, 0 on error */
struct lump* insert_new_lump(struct lump** list, char* new_hdr,
								int len, int type)
{
	struct lump* tmp;

	tmp=malloc(sizeof(struct lump));
	if (tmp==0){
		LOG(L_ERR, "ERROR: insert_new_lump: out of memory\n");
		return 0;
	}
	memset(tmp,0,sizeof(struct lump));
	tmp->next=*list;
	tmp->type=type;
	tmp->op=LUMP_ADD;
	tmp->u.value=new_hdr;
	tmp->len=len;
	*list=tmp;
	return tmp;
}



/* inserts a  header/data lump immediately after hdr 
 * returns pointer on success, 0 on error */
struct lump* insert_new_lump_after( struct lump* after, char* new_hdr,
							int len, int type)
{
	struct lump* tmp;

	tmp=malloc(sizeof(struct lump));
	if (tmp==0){
		LOG(L_ERR, "ERROR: insert_new_lump_after: out of memory\n");
		return 0;
	}
	memset(tmp,0,sizeof(struct lump));
	tmp->after=after->after;
	tmp->type=type;
	tmp->op=LUMP_ADD;
	tmp->u.value=new_hdr;
	tmp->len=len;
	after->after=tmp;
	return tmp;
}



/* inserts a  header/data lump immediately before "before" 
 * returns pointer on success, 0 on error */
struct lump* insert_new_lump_before( struct lump* before, char* new_hdr,
							int len, int type)
{
	struct lump* tmp;

	tmp=malloc(sizeof(struct lump));
	if (tmp==0){
		LOG(L_ERR,"ERROR: insert_new_lump_before: out of memory\n");
		return 0;
	}
	memset(tmp,0,sizeof(struct lump));
	tmp->before=before->before;
	tmp->type=type;
	tmp->op=LUMP_ADD;
	tmp->u.value=new_hdr;
	tmp->len=len;
	before->before=tmp;
	return tmp;
}



/* removes an already existing header/data lump */
struct lump* del_lump(struct lump** list, int offset, int len, int type)
{
	struct lump* tmp;
	struct lump* prev, *t;

	tmp=malloc(sizeof(struct lump));
	if (tmp==0){
		LOG(L_ERR, "ERROR: insert_new_lump_before: out of memory\n");
		return 0;
	}
	memset(tmp,0,sizeof(struct lump));
	tmp->op=LUMP_DEL;
	tmp->type=type;
	tmp->u.offset=offset;
	tmp->len=len;
	prev=0;
	for (t=*list;t; prev=t, t=t->next){
		/* insert it sorted after offset */
		if (((t->op==LUMP_DEL)||(t->op==LUMP_NOP))&&(t->u.offset>offset))
			break;
	}
	tmp->next=t;
	if (prev) prev->next=tmp;
	else *list=tmp;
	return tmp;
}



/* add an anhor */
struct lump* anchor_lump(struct lump** list, int offset, int len, int type)
{
	struct lump* tmp;
	struct lump* prev, *t;

	tmp=malloc(sizeof(struct lump));
	if (tmp==0){
		LOG(L_ERR, "ERROR: insert_new_lump_before: out of memory\n");
		return 0;
	}
	memset(tmp,0,sizeof(struct lump));
	tmp->op=LUMP_NOP;
	tmp->type=type;
	tmp->u.offset=offset;
	tmp->len=len;
	prev=0;
	DBG("anchor: new tmp=%x\n", tmp);
	for (t=*list;t; prev=t, t=t->next){
		/* insert it sorted after offset */
		if (((t->op==LUMP_DEL)||(t->op==LUMP_NOP))&&(t->u.offset>offset))
			break;
	}
	DBG("anchor: inserting it between %x and %x (*list=%x)\n", prev, t,*list);
	tmp->next=t;
	
	if (prev) prev->next=tmp;
	else *list=tmp;
	return tmp;
}



void free_lump(struct lump* lmp)
{
	DBG("- free_lump(%x), op=%d\n", lmp, lmp->op);
	if (lmp && (lmp->op==LUMP_ADD)){
		if (lmp->u.value) free(lmp->u.value);
		lmp->u.value=0;
		lmp->len=0;
	}
	DBG("- exiting free_lump(%x)\n", lmp);
}



void free_lump_list(struct lump* l)
{
	struct lump* t, *crt;
	t=l;
	DBG("+ free_lump_list(%x)\n", l);
	while(t){
		crt=t;
		t=t->next;
		DBG("free_lump_list: freeing %x\n", crt);
		/* dangerous recursive clean*/
		if (crt->before) free_lump_list(crt->before);
		if (crt->after)  free_lump_list(crt->after);
		DBG("free_lump_list: back from recursive calls (%x) \n", crt);
		/*clean current elem*/
		free_lump(crt);
		free(crt);
		DBG("after free_lump_list: %x\n", crt);
	}
	DBG("+ exiting free_lump_list(%x)\n", l);
}
