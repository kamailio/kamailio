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

#include <presence/domain_maintainer.h>
#include <cds/ptr_vector.h>
#include <cds/memory.h>
#include <cds/logger.h>

domain_maintainer_t *create_domain_maintainer()
{
	domain_maintainer_t *dm;

	dm = (domain_maintainer_t*)cds_malloc(sizeof(domain_maintainer_t));
	if (dm) {
		ptr_vector_init(&dm->registered_domains, 8);
		cds_mutex_init(&dm->mutex);
		dm->rc_grp = create_reference_counter_group(16);
	}
	return dm;
}

void destroy_domain_maintainer(domain_maintainer_t *dm) 
{
	int i, cnt;
	notifier_domain_t *d;
	
	if (!dm) return;
			
	DEBUG_LOG("destroying domain maintainer\n");

	cnt = ptr_vector_size(&dm->registered_domains);
	for (i = 0; i < cnt; i++) {
		d = ptr_vector_get(&dm->registered_domains, i);
		if (!d) continue;
		if (remove_reference(&d->ref)) {
			DEBUG_LOG("freeing domain: \'%.*s\'\n", FMT_STR(d->name));
			destroy_notifier_domain(d);
		}
	}
	ptr_vector_destroy(&dm->registered_domains);
	cds_mutex_destroy(&dm->mutex);
	cds_free(dm);
}

static notifier_domain_t *find_domain_nolock(domain_maintainer_t *dm, const str_t *name)
{
	notifier_domain_t *d = NULL;
	int i, cnt;
	
	cnt = ptr_vector_size(&dm->registered_domains);
	for (i = 0; i < cnt; i++) {
		d = ptr_vector_get(&dm->registered_domains, i);
		if (!d) continue;
		if (str_case_equals(&d->name, name) == 0) return d;
	}
	return NULL;
}

static notifier_domain_t *add_domain_nolock(domain_maintainer_t *dm, const str_t *name)
{
	notifier_domain_t *d = create_notifier_domain(dm->rc_grp, name);
	
	if (d) {
		DEBUG_LOG("created domain: \'%.*s\'\n", FMT_STR(d->name));
		ptr_vector_add(&dm->registered_domains, d);
		return d;
	}
	else return NULL;
}

/* notifier_domain_t *find_notifier_domain(domain_maintainer_t *dm, const str_t *name)
{
	notifier_domain_t *d = NULL;
	
	if (!dm) return NULL;
	cds_mutex_lock(&dm->mutex);
	d = find_domain_nolock(dm, name);
	cds_mutex_unlock(&dm->mutex);
	return d;
} */

notifier_domain_t *register_notifier_domain(domain_maintainer_t *dm, const str_t *name)
{
	notifier_domain_t *d = NULL;
	
	if (!dm) return NULL;
	
	cds_mutex_lock(&dm->mutex);
	d = find_domain_nolock(dm, name);
	if (!d) d = add_domain_nolock(dm, name);
	if (d) {
		add_reference(&d->ref); /* add reference for client */
	}
	cds_mutex_unlock(&dm->mutex);
	return d;
}

static void remove_notifier_domain(domain_maintainer_t *dm, notifier_domain_t *domain)
{
	notifier_domain_t *d = NULL;
	int i, cnt;
	
	cnt = ptr_vector_size(&dm->registered_domains);
	for (i = 0; i < cnt; i++) {
		d = ptr_vector_get(&dm->registered_domains, i);
		if (d == domain) {
			ptr_vector_remove(&dm->registered_domains, i);
			break;
		}
	}
}

void release_notifier_domain(domain_maintainer_t *dm, notifier_domain_t *domain)
{
	if ((!dm) || (!domain)) return;
	
	cds_mutex_lock(&dm->mutex);
	if (remove_reference(&domain->ref)) {
		/* last reference */
		DEBUG_LOG("freeing domain: \'%.*s\'\n", FMT_STR(domain->name));
		remove_notifier_domain(dm, domain);
		destroy_notifier_domain(domain);
	}
	cds_mutex_unlock(&dm->mutex);
}

