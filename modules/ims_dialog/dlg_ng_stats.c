#include "dlg_ng_stats.h"

struct dialog_ng_counters_h dialog_ng_cnts_h;

/* dialog_ng counters definitions */
counter_def_t dialog_ng_cnt_defs[] =  {
	{&dialog_ng_cnts_h.active, "active", 0, 0, 0,
		"number of current active (answered) dialogs"},
	{&dialog_ng_cnts_h.early, "early", 0, 0, 0,
		"number of current early dialogs"},
	{&dialog_ng_cnts_h.expired, "expired", 0, 0, 0,
		"number of expired dialogs (forcibly killed)"},
	{&dialog_ng_cnts_h.processed, "processed", 0, 0, 0,
		"number of processed dialogs"},
	{0, 0, 0, 0, 0, 0 }
};

/** intialize dialog_ng statistics.
 * @return < 0 on errror, 0 on success.
 */
int dialog_ng_stats_init()
{
	if (counter_register_array("dialog_ng", dialog_ng_cnt_defs) < 0)
		goto error;
	return 0;
error:
	return -1;
}


void dialog_ng_stats_destroy()
{
	/* do nothing */
}