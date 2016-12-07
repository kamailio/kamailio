#ifndef DIALOG_NG_STATS_H
#define	DIALOG_NG_STATS_H

#include "../../core/counters.h"

struct dialog_ng_counters_h {
    counter_handle_t active;
    counter_handle_t early;
    counter_handle_t expired;
    counter_handle_t processed;

};

struct dialog_ng_counters_h dialog_ng_cnts_h;

int dialog_ng_stats_init();
void dialog_ng_stats_destroy();

#endif	/* DIALOG_NG_STATS_H */