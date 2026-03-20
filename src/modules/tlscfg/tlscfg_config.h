/*
 * tlscfg module - TLS profile management companion for the tls module
 *
 * Copyright (C) 2026 Aurora Innovation
 *
 * Author: Daniel Donoghue
 *
 * This file is part of Kamailio, a free SIP server.
 *
 * SPDX-License-Identifier: GPL-2.0-or-later
 *
 * Kamailio is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version
 *
 * Kamailio is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA  02110-1301  USA
 */

/**
 * @file tlscfg_config.h
 * @brief Config file parser and atomic writer declarations
 * @ingroup tlscfg
 */

#ifndef _TLSCFG_CONFIG_H_
#define _TLSCFG_CONFIG_H_

#include "../../core/str.h"

#include "tlscfg_profile.h"

/**
 * @brief Parse a tls.cfg file into the in-memory profile index
 * @param path path to the tls.cfg file
 * @return 0 on success, -1 on error
 */
int tlscfg_config_load(str *path);

/**
 * @brief Write the in-memory profile index back to the tls.cfg file
 *
 * Uses atomic write (tmp + rename). Disabled profiles are written as
 * comments so the tls module's parser skips them.
 *
 * @param path path to the tls.cfg file
 * @return 0 on success, -1 on error
 */
int tlscfg_config_write(str *path);

/**
 * @brief Get the last-recorded mtime of the config file
 * @return mtime of last read/write, or 0 if never accessed
 */
time_t tlscfg_config_get_mtime(void);

#endif /* _TLSCFG_CONFIG_H_ */
