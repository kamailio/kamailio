/*
 * KEMI export for the originating registration check.
 * SPDX-License-Identifier: GPL-2.0-or-later
 *
 * As a special exception, the copyright holders permit linking this
 * contribution with the OpenSSL library and distributing the resulting
 * combined work. The GNU General Public License applies to all other code.
 */

#include <string.h>
#include "../../core/kemi.h"
#include "../../core/mem/mem.h"
#include "lookup.h"

extern usrloc_api_t ul;

/** Use the same admission check as the native configuration export. */
static int ki_orig_impu_has_contact(sip_msg_t *msg, str *domain_name)
{
	udomain_t *d = NULL;
	char *name;
	int ret;

	if(!msg || !domain_name || !domain_name->s || domain_name->len <= 0
			|| memchr(domain_name->s, '\0', domain_name->len))
		return -1;

	/* The usrloc domain lookup requires a zero-terminated string. */
	name = pkg_malloc((unsigned int)domain_name->len + 1U);
	if(!name) {
		PKG_MEM_ERROR;
		return -1;
	}
	memcpy(name, domain_name->s, domain_name->len);
	name[domain_name->len] = '\0';
	ret = ul.get_udomain(name, &d);
	pkg_free(name);
	if(ret < 0 || !d) {
		LM_ERR("usrloc domain [%.*s] is not registered\n", domain_name->len,
				domain_name->s);
		return -1;
	}

	return orig_impu_has_contact(msg, (char *)d, NULL);
}

/* clang-format off */
static sr_kemi_t sr_kemi_ims_registrar_scscf_exports[] = {
	{ str_init("ims_registrar_scscf"), str_init("orig_impu_has_contact"),
		SR_KEMIP_INT, ki_orig_impu_has_contact,
		{ SR_KEMIP_STR, SR_KEMIP_NONE, SR_KEMIP_NONE,
			SR_KEMIP_NONE, SR_KEMIP_NONE, SR_KEMIP_NONE }
	},
	{ {0, 0}, {0, 0}, 0, NULL, {0, 0, 0, 0, 0, 0} }
};
/* clang-format on */

int mod_register(char *path, int *dlflags, void *p1, void *p2)
{
	(void)path;
	(void)dlflags;
	(void)p1;
	(void)p2;
	return sr_kemi_modules_add(sr_kemi_ims_registrar_scscf_exports);
}
