/*
 * Accounting module
 *
 * $Id$
 */

#ifndef _ACC_H
#define _ACC_H

/* module parameter declaration */
extern int use_db;
extern char *db_url;
extern char *uid_column;
extern char *db_table;
extern int log_level;
extern int early_media;
extern int failed_transactions;
extern int flagged_only;

#endif
