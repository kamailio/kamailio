/*
 * $Id$
 *
 * Process Table
 *
 *
 */

#ifndef _PT_H

#include <sys/types.h>
#include <unistd.h>

#include "globals.h"
#include "timer.h"

#define MAX_PT_DESC	128

struct process_table {
	int pid;
	char desc[MAX_PT_DESC];
};

extern struct process_table *pt;
extern int process_no;

/* get number of process started by main with
   given configuration
*/
inline static int process_count()
{
    return 
		/* receivers and attendant */
		(dont_fork ? 1 : children_no*sock_no + 1)
		/* timer process */
		+ (timer_list ? 1 : 0 )
		/* fifo server */
		+((fifo==NULL || strlen(fifo)==0) ? 0 : 1 );
}


/* retun processes's pid */
inline static int my_pid()
{
	return pt ? pt[process_no].pid : getpid();
}


#endif
