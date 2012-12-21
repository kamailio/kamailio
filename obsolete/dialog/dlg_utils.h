#ifndef __DLG_UTILS_H
#define __DLG_UTILS_H

#include "dlg_mod.h"

int preset_dialog_route(dlg_t* dialog, str *route);
int bind_dlg_mod(dlg_func_t *dst);
int cmp_dlg_ids(dlg_id_t *a, dlg_id_t *b);
unsigned int hash_dlg_id(dlg_id_t *id);

#endif
