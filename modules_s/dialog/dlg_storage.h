#ifndef __DLG_STORAGE_H
#define __DLG_STORAGE_H

#include "dlg_mod.h"

typedef struct {
	dlg_t *dialog;
	/* avps */
	/* ... */
} dialog_info_t;

/* initializes dialog storage */
int init_dlg_storage(int db_mode, const str *db_url);
int init_dlg_storage_child(int db_mode, const str *db_url);
void destroy_dlg_storage();

int add_dialog(dialog_info_t *info);
dialog_info_t *find_dialog(dlg_id_t *id);

#endif
