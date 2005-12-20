#ifndef _SELECT_CORE_H
#define _SELECT_CORE_H

#include "str.h"
#include "parser/msg_parser.h"
#include "select.h"

static select_row_t select_core[] = {
	{ NULL, PARAM_INT, STR_NULL, NULL, 0}
};

static select_table_t select_core_table = {select_core, NULL};

#endif // _SELECT_CORE_H
