/*
 *
 * $Id$
 *
 *
 */

#ifndef _UL_FIFO_H
#define _UL_FIFO_H

/* FIFO commands */
#define UL_STATS	"ul_stats"
#define UL_RM		"ul_rm"

/* buffer dimensions */
#define MAX_TABLE 128
#define MAX_USER 256

int init_ul_fifo(void);

#endif
