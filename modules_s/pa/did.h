#ifndef DID_H
#define DID_H

#include "/home/janakj/sip_router/str.h"
#include "/home/janakj/sip_router/parser/msg_parser.h"
#include <stdio.h>


/*
 * Structure representing dialog ID
 */
typedef struct did {
	str cid;     /* Call-ID */
	str lt;      /* Local tag */
	str rt;      /* Remote tag */
} did_t;


/*
 * Create a new dialog ID from the given parameters
 */
int new_did(did_t* _d, str* _cid, str* _lt, str* _rt);


/*
 * Free memory associated with dialog ID
 */
void free_did(did_t* _d);


/*
 * Match a dialog ID against a SIP message
 */
int match_did(did_t* _d, str* _cid, str* _lt, str* _rt);


/*
 * Just for debugging
 */
void print_did(FILE* _o, did_t* _d);


#endif /* DID_H */
