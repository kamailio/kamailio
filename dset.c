/*
 * $Id$
 *
 * destination set
 */

#include <string.h>

#include "dprint.h"
#include "config.h"
#include "parser/parser_f.h"
#include "parser/msg_parser.h"
#include "ut.h"
#include "hash_func.h"
#include "dset.h"



/* where we store URIs of additional transaction branches
  (-1 because of the default branch, #0)
*/
static struct branch branches[ MAX_BRANCHES - 1 ];
/* how many of them we have */
static unsigned int nr_branches=0;
/* branch iterator */
static int branch_iterator=0;

void init_branch_iterator(void)
{
	branch_iterator=0;
}

char *next_branch( int *len )
{
	unsigned int i;

	i=branch_iterator;
	if (i<nr_branches) {
		branch_iterator++;
		*len=branches[i].len;
		return branches[i].uri;
	} else {
		*len=0;
		return 0;
	}
}

void clear_branches()
{
	nr_branches=0;
}

/* add a new branch to current transaction */
int append_branch( struct sip_msg *msg, char *uri, int uri_len )
{
	/* if we have already set up the maximum number
	   of branches, don't try new ones */
	if (nr_branches==MAX_BRANCHES-1) {
		LOG(L_ERR, "ERROR: append_branch: max nr of branches exceeded\n");
		return -1;
	}

	if (uri_len>MAX_URI_SIZE-1) {
		LOG(L_ERR, "ERROR: append_branch: too long uri: %.*s\n",
			uri_len, uri );
		return -1;
	}

	/* if not parameterized, take current uri */
	if (uri==0) {
		if (msg->new_uri.s) { 
			uri=msg->new_uri.s;
			uri_len=msg->new_uri.len;
		} else {
			uri=msg->first_line.u.request.uri.s;
			uri_len=msg->first_line.u.request.uri.len;
		}
	}
	
	memcpy( branches[nr_branches].uri, uri, uri_len );
	/* be safe -- add zero termination */
	branches[nr_branches].uri[uri_len]=0;
	branches[nr_branches].len=uri_len;
	
	nr_branches++;
	return 1;
}
