#ifndef DIALOG_H
#define DIALOG_H

#include "/home/janakj/sip_router/str.h"
#include "dstate.h" /* Dialog state type */
#include "routeset.h"
#include "did.h"
#include "/home/janakj/sip_router/parser/msg_parser.h"
#include <stdio.h>

#define dmalloc pkg_malloc
#define dfree pkg_free

#define UAS 1
#define UAC 0


/*
 * Structure representing dialog
 */
typedef struct dialog {
	did_t id;             /* Dialog ID - callid, local tag, remote tag */
	unsigned int lseq;    /* Local sequence number */
	unsigned int rseq;    /* Remote sequence number */
	str luri;             /* Local URI */
	str ruri;             /* Remote URI */
	str rtarget;          /* Remote Target - Contact */
	unsigned char secure; /* Secure flag */
	routeset_t rs;        /* Route set */
	dstate_t state;       /* Dialog state */
} dialog_t;


/*
 * Create a dialog as UAS - i.e. upon receiving a request
 */
int new_dialog(dialog_t** _d, struct sip_msg* _m, int _t);


/*
 * Match a dialog
 */
int match_dialog(dialog_t* _d, struct sip_msg* _m);


/*
 * Free all memory associated with a dialog
 */
void free_dialog(dialog_t* _d);


/*
 * Just for debugging
 */
void print_dialog(FILE* _o, dialog_t* _d);


#endif /* DIALOG_H */
