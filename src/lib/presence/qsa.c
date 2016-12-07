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

#include <presence/qsa.h>
#include <cds/logger.h>
#include <cds/cds.h>
#include <presence/domain_maintainer.h>

typedef struct {
	int init_cnt;
	domain_maintainer_t *dm;
} init_data_t;

static init_data_t *init = NULL;

int qsa_initialize()
{
	int res = 0;

	/* initialization should be called from one process/thread 
	 * it is not synchronized because it is impossible ! */
	if (!init) {
		init = (init_data_t*)cds_malloc(sizeof(init_data_t));
		if (!init) return -1;
		init->init_cnt = 0;
	}

	if (init->init_cnt > 0) { /* already initialized */
		init->init_cnt++;
		return 0;
	}
	else {
		DEBUG_LOG("init the content\n");

		/* !!! put the real initialization here !!! */
		init->dm = create_domain_maintainer();
		if (!init->dm) {
			ERROR_LOG("qsa_initialize error - can't initialize domain maintainer\n");
			res = -1;
		}
	}
			
	if (res == 0) init->init_cnt++;
	return res;
}

void qsa_cleanup() 
{
	if (init) {
		if (--init->init_cnt == 0) {
			DEBUG_LOG("cleaning the content\n");
			
			/* !!! put the real destruction here !!! */
			if (init->dm) destroy_domain_maintainer(init->dm);
			
			cds_free(init);
			init = NULL;
		}
	}
}

notifier_domain_t *qsa_register_domain(const str_t *name)
{
	notifier_domain_t *d = NULL;

	if (!init) {
		ERROR_LOG("qsa_initialize was not called - can't register domain\n");
		return NULL;
	}
	if (init->dm) d = register_notifier_domain(init->dm, name);
	return d;
}

notifier_domain_t *qsa_get_default_domain()
{
	return qsa_register_domain(NULL);
}

void qsa_release_domain(notifier_domain_t *domain)
{
	if (init) 
		if (init->dm) release_notifier_domain(init->dm, domain);
}
