#include <cds/dbid.h>
#include <time.h>
#include <stdlib.h>
#include <sys/types.h>
#include <unistd.h>
#include <stdio.h>

void generate_dbid_ptr(dbid_t dst, void *data_ptr)
{
	/* TODO: add cluster distinctive member */
	/* FIXME: replace sprintf by something more effective */
	snprintf(dst, sizeof(dst), "%px%xx%x", 
			data_ptr, (int)time(NULL), rand());
}

#ifdef SER

/* only for SER (not for apps with threads) */
void generate_dbid(dbid_t dst)
{
	static int cntr = 0;
	static pid_t my_pid = -1;
	
	if (my_pid < 0) {
		my_pid = getpid();
	}

	/* TODO: add cluster distinctive member */
	/* FIXME: replace sprintf by something more effective */
	snprintf(dst, sizeof(dst), "%xy%xy%xy%x", 
			my_pid, cntr++, (int)time(NULL), rand());
}

#endif

