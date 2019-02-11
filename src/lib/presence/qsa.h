/* 
 * Copyright (C) 2005 iptelorg GmbH
 *
 * This file is part of ser, a free SIP server.
 *
 * ser is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version
 *
 * For a license to use the ser software under conditions
 * other than those described here, or to purchase support for this
 * software, please contact iptel.org by e-mail at the following addresses:
 *    info@iptel.org
 *
 * ser is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA  02110-1301  USA
 */

#ifndef __QSA_H
#define __QSA_H

#ifdef __cplusplus
extern "C" {
#endif

#include <cds/sstr.h>
#include <presence/notifier_domain.h>
	
int qsa_initialize();
void qsa_cleanup();

notifier_domain_t *qsa_register_domain(const str_t *name);
notifier_domain_t *qsa_get_default_domain();
void qsa_release_domain(notifier_domain_t *domain);

#ifdef __cplusplus
}
#endif
	
#endif
