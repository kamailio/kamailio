/*
 * $Id$
 */

#ifndef _T_FORKS_H
#define _T_FORKS_H

#include "../../ip_addr.h"
#include "../../str.h"


struct fork
{
    union sockaddr_union to;
    char inactive;
    unsigned char free_flag;
    str           uri;

};

extern struct fork      t_forks[ NR_OF_CLIENTS ];
extern unsigned int     nr_forks;

int t_add_fork( union sockaddr_union to, char* uri_s,
				unsigned int uri_len, enum fork_type type,
				unsigned char free_flag);
int t_clear_forks();


#endif
