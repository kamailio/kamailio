/* dialog functions accesible from script */

#ifndef __DIALOG_HANDLER_H
#define __DIALOG_HANDLER_H

#include "dlg_mod.h"

/* save dialog with AVPs into memory (and probably DB) 
 * so it will be accessible through hashtable */
int handle_save_dialog(struct sip_msg* _m);

/* load dialog saved by save_dialog (reloads AVPs, ...) */
int handle_load_dialog(struct sip_msg* _m);

/* remove saved dialog */
int handle_remove_dialog(struct sip_msg* _m);

#endif
