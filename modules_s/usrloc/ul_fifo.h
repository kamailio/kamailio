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
#define UL_RM_CONTACT   "ul_rm_contact"
#define UL_DUMP         "ul_dump"
#define UL_FLUSH        "ul_flush"
#define UL_ADD          "ul_add"
#define UL_SHOW_CONTACT "ul_show_contact"

/* buffer dimensions */
#define MAX_TABLE 128
#define MAX_USER 256

int init_ul_fifo(void);

#endif
