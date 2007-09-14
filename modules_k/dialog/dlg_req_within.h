#ifndef DLG_REQUEST_WITHIN_H
#define DLG_REQUEST_WITHIN_H

#define MAX_FWD			70
#define MAX_SIZE		256
#define RCV_BYE_REPLY	1

#define	MI_DIALOG_NOT_FOUND 		"Requested Dialog not found"
#define MI_DIALOG_NOT_FOUND_LEN 	(sizeof(MI_DIALOG_NOT_FOUND)-1)
#define MI_DLG_OPERATION_ERR		"Operation failed"
#define MI_DLG_OPERATION_ERR_LEN	(sizeof(MI_DLG_OPERATION_ERR)-1)

extern struct tm_binds d_tmb;
extern int dlg_enable_stats;
extern stat_var * active_dlgs;
int send_bye(struct dlg_cell *, int);
struct mi_root * mi_terminate_dlg(struct mi_root *cmd_tree, void *param );

#endif
