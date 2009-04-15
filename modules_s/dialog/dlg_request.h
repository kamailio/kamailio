#ifndef __DIALOG_REQUEST_H
#define __DIALOG_REQUEST_H

#include "dlg_mod.h"
#include "../../modules/tm/t_hooks.h"

int request_outside(str* method, str* headers, str* body, dlg_t* dialog, transaction_cb cb, void* cbp);
int request_inside(str* method, str* headers, str* body, dlg_t* dialog, transaction_cb completion_cb, void* cbp);

#endif
