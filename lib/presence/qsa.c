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

#include <presence/qsa.h>
#include <cds/logger.h>
#include <presence/domain_maintainer.h>

static domain_maintainer_t *dm = NULL;
static int initialized = 0;

int qsa_initialize()
{
	if (!initialized) {
		dm = create_domain_maintainer();
		if (dm) initialized = 1;
		else {
			ERROR_LOG("qsa_initialize error - can't initialize domain maintainer\n");
			return -1;
		}
		DEBUG_LOG("QSA initialized\n");
	}
	return 0;
}

void qsa_cleanup() 
{
	if (initialized && dm) {
		destroy_domain_maintainer(dm);
		dm = NULL;
		initialized = 0;
	}
}

notifier_domain_t *qsa_register_domain(const str_t *name)
{
	notifier_domain_t *d = NULL;

	if (!dm) {
		ERROR_LOG("qsa_initialize was not called - can't register domain\n");
		return NULL;
	}
	d = register_notifier_domain(dm, name);
	return d;
}

notifier_domain_t *qsa_get_default_domain()
{
	return qsa_register_domain(NULL);
}

void qsa_release_domain(notifier_domain_t *domain)
{
	if (dm) release_notifier_domain(dm, domain);
}
