/*
 * $Id$
 *
 * adding/removing headers or any other data chunk from a message
 */

#ifndef data_lump_h
#define data_lump_h


enum { LUMP_NOP=0, LUMP_DEL, LUMP_ADD };

struct lump{
	int type; /* VIA, OTHER, UNSPEC(=0), ... */
	int op;   /* DEL, ADD, NOP, UNSPEC(=0) */
	
	union{
		int offset; /* used for DEL, MODIFY */
		char * value; /* used for ADD */
	}u;
	int len; /* length of this header field */
	
	
	struct lump* before; /* list of headers to be inserted in front of the
								current one */
	struct lump* after; /* list of headers to be inserted immediately after
							  the current one */
	
	struct lump* next;
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



/* adds a header to the end */
struct lump* append_new_lump(struct lump** list, char* new_hdr,
							 int len, int type);
/* inserts a header to the beginning */
struct lump* insert_new_lump(struct lump** list, char* new_hdr,
							  int len, int type);
struct lump* insert_new_lump_after(struct lump* after,
									char* new_hdr, int len, int type);
struct lump* insert_new_lump_before(struct lump* before, char* new_hdr,
									int len,int type);


/* removes an already existing header */
struct lump* del_lump(struct lump** list, int offset, int len, int type);
/* set an anchor */
struct lump* anchor_lump(struct lump** list, int offset, int len, int type);


/* frees the content of a lump struct */
void free_lump(struct lump* l);
/*frees an entire lump list, recursively */
void free_lump_list(struct lump* lump_list);

#endif
