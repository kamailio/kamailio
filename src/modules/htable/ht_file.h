/**
 *
 * Copyright (C) 2026 The Kamailio Project
 *
 * This file is part of Kamailio, a free SIP server.
 *
 * SPDX-License-Identifier: GPL-2.0-or-later
 */

#ifndef _HT_FILE_H_
#define _HT_FILE_H_

#include "ht_api.h"

int ht_file_load_table(ht_t *ht);
int ht_file_save_table(ht_t *ht);
int ht_file_load_tables(void);
int ht_file_sync_tables(void);

#endif
