/*
 * $Id$
 *
 * forking requests
 */

#include "../../dprint.h"
#include "../../config.h"
#include "../../parser/parser_f.h"
#include "../../ut.h"
#include "hash_func.h"
#include "t_funcs.h"
#include "t_fork.h"



unsigned int     nr_forks;
struct fork      t_forks[ NR_OF_CLIENTS ];


int t_add_fork( union sockaddr_union to, char* uri_s,
			unsigned int uri_len, enum fork_type type, 
			unsigned char free_flag)
{
	unsigned int pos=0;
	char         *foo=0;

	switch (type)
	{
		case DEFAULT:
			if (nr_forks+1>=MAX_FORK)
			{
				LOG(L_ERR,"ERROR:t_add_fork: trying to add new fork ->"
					" MAX_FORK exceded\n");
				return -1;
			}
			pos = ++nr_forks;
			break;
		case NO_RESPONSE:
			/* v6; -Jiri
			if (t_forks[NO_RPL_BRANCH].ip)
			*/
			if (!t_forks[NO_RPL_BRANCH].inactive)
				LOG(L_WARN,"WARNING:t_add_fork: trying to add NO_RPL fork ->"
					" it was set before -> overriding\n");
			if (uri_s && uri_len)
			{
				foo = (char*)shm_malloc(uri_len);
				if (!foo)
				{
					LOG(L_ERR,"ERROR:t_add_fork: cannot get free memory\n");
					return -1;
				}
				memcpy(foo,uri_s,uri_len);
			}
			if (free_flag && uri_s)
				pkg_free(uri_s);
			uri_s = foo;
			free_flag = 0;
			pos = NO_RPL_BRANCH;
	}
	/* -v6
	t_forks[pos].ip = ip;
	t_forks[pos].port = port;
	*/
	t_forks[pos].to=to;

	if (uri_s && uri_len)
	{
		t_forks[pos].free_flag = free_flag;
		t_forks[pos].uri.len = uri_len;
		t_forks[pos].uri.s = uri_s;
	}

	return 1;
}




int t_clear_forks( )
{
	int i;

	DBG("DEBUG: t_clear_forks: clearing tabel...\n");
	for(i=1;i<nr_forks;i++)
		if (t_forks[i].free_flag && t_forks[i].uri.s)
			pkg_free(t_forks[i].uri.s);
	memset( t_forks, 0, sizeof(t_forks));
	nr_forks = 0;
	return 1;
}



