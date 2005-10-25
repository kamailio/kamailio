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
 * Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307  USA
 */

#ifndef __DOMAIN_MAINTAINER_H
#define __DOMAIN_MAINTAINER_H

#ifdef __cplusplus
extern "C" {
#endif

#include <presence/notifier_domain.h>
#include <cds/ptr_vector.h>
#include <cds/sync.h>

typedef struct {
	ptr_vector_t registered_domains;
	cds_mutex_t mutex;
} domain_maintainer_t;

	
domain_maintainer_t *create_domain_maintainer();
void destroy_domain_maintainer(domain_maintainer_t *dm);

notifier_domain_t *register_notifier_domain(domain_maintainer_t *dm, const str_t *name);
void release_notifier_domain(domain_maintainer_t *dm, notifier_domain_t *domain);
	
#ifdef __cplusplus
}
#endif

#endif
