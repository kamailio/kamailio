/*
 * $Id$
 */

#ifndef _T_FORKS_H
#define _T_FORKS_H

#include "config.h"

#define CONTACT "Contact: "
#define CONTACT_LEN 9
#define CONTACT_DELIM ", "
#define CONTACT_DELIM_LEN 2

char *create_dset( struct sip_msg *msg, int *len );

struct branch
{
	char uri[MAX_URI_SIZE];
	unsigned int len;
};

struct sip_msg;

/*
typedef int (*tfork_f)( struct sip_msg *msg, char *uri, int uri_len );
*/

/* add a new branch to current transaction */
int append_branch( struct sip_msg *msg, char *uri, int uri_len );
/* iterate through list of new transaction branches */
void init_branch_iterator();
char *next_branch( int *len );
void clear_branches();

#endif
