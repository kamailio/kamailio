/**
 * Copyright 2016 (C) Orange
 * <camille.oudot@orange.com>
 *
 * This file is part of Kamailio, a free SIP server.
 *
 * This file is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version
 *
 *
 * This file is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA  02110-1301  USA
 *
 */

#include <stdio.h>
#include <systemd/sd-journal.h>

#include "../../core/xavp.h"
#include "../../core/dprint.h"
#include "../../core/mem/pkg.h"

#include "journal_send.h"

int k_sd_journal_send_xavp(str *rname)
{
	struct _sr_xavp *rxavp, *nxavp;
	int cnt, buflen, i, ret;
	struct iovec *logv;

	rxavp = xavp_get(rname, NULL);

	if (!rxavp || rxavp->val.type != SR_XTYPE_XAVP) {
		LM_ERR("not a valid xavp: %.*s?\n", rname->len, rname->s);
		return -1;
	}

	/* first, count xavp nodes */
	for (nxavp = rxavp->val.v.xavp, cnt = 0;
			nxavp;
			nxavp = nxavp->next, cnt++);

	if (cnt == 0) {
		/* nothing to log? */
		LM_NOTICE("empty xavp: %.*s?, no log event was sent to journald\n",
				rname->len,
				rname->s);
		return 1;
	}

	logv = pkg_malloc(cnt * sizeof (struct iovec));

	if (!logv) {
		LM_ERR("failed to allocate pkg memory\n");
		return -1;
	}

	ret = -1;

	for (nxavp = rxavp->val.v.xavp, cnt = 0;
			nxavp;
			nxavp = nxavp->next, cnt++)
	{
		if (nxavp->val.type == SR_XTYPE_LONG) {
			buflen = snprintf(NULL, 0,
					"%.*s=%ld",
					nxavp->name.len, nxavp->name.s,
					nxavp->val.v.l);
			logv[cnt].iov_base = pkg_malloc(buflen + 1/*snprintf's trailing \0*/);
			if (!logv[cnt].iov_base ) {
				LM_ERR("failed to allocate pkg memory\n");
				goto free;
			}
			logv[cnt].iov_len = buflen;

			snprintf(logv[cnt].iov_base, buflen + 1,
					"%.*s=%ld",
					nxavp->name.len, nxavp->name.s,
					nxavp->val.v.l);
		} else if (nxavp->val.type == SR_XTYPE_STR) {
			buflen = nxavp->name.len + 1 + nxavp->val.v.s.len;
			logv[cnt].iov_base = pkg_malloc(buflen);
			if (!logv[cnt].iov_base) {
				LM_ERR("failed to allocate pkg memory\n");
				goto free;
			}
			logv[cnt].iov_len = buflen;

			memcpy(logv[cnt].iov_base, nxavp->name.s, nxavp->name.len);
			((char*)logv[cnt].iov_base)[nxavp->name.len] = '=';
			memcpy(((char*)(logv[cnt].iov_base)) + nxavp->name.len + 1,
					nxavp->val.v.s.s,
					nxavp->val.v.s.len);
		} else {
			LM_NOTICE("unsupported type %d for field %.*s, skipped...\n",
					nxavp->val.type,
					nxavp->name.len, nxavp->name.s);
			logv[cnt].iov_len = 0;
			logv[cnt].iov_base = NULL;
		}
		LM_DBG("added io slice %d: %.*s\n", cnt, (int)logv[cnt].iov_len, (char*)logv[cnt].iov_base);
	}

	if (sd_journal_sendv(logv, cnt) != 0) {
		LM_ERR("sd_journal_sendv() failed\n");
	} else {
		ret = 1;
	}

free:
	for (i = 0 ; i < cnt ; i++) {
		if (logv[i].iov_base) pkg_free(logv[i].iov_base);
	}
	pkg_free(logv);
	return ret;
}
