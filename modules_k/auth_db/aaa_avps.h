/*
 * $Id$
 *
 * Copyright (C) 2005 Voice Sistem SRL
 *
 * This file is part of Kamailio, a free SIP server.
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
 * Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307  USA
 *
 * History:
 * --------
 *  2005-05-31 created by bogdan
 */

#ifndef _OPENSER_AAA_AVPS_H_
#define _OPENSER_AAA_AVPS_H_

#include "../../mem/mem.h"
#include "../../dprint.h"
#include "../../trim.h"
#include "../../pvar.h"

#include <string.h>


struct aaa_avp {
	int_str avp_name;
	unsigned short avp_type;
	str attr_name;
	struct aaa_avp *next;
};



static inline void free_aaa_avp(struct aaa_avp *avp)
{
	if (avp) {
		if (avp->avp_type&AVP_NAME_STR && avp->avp_name.s.s!=avp->attr_name.s)
			pkg_free(avp->avp_name.s.s);
		if (avp->attr_name.s)
			pkg_free(avp->attr_name.s);
		pkg_free(avp);
	}
}



static inline void free_aaa_avp_list(struct aaa_avp *avp)
{
	struct aaa_avp *tmp;

	while (avp) {
		tmp = avp;
		avp = avp->next;
		free_aaa_avp( tmp );
	}
}



static inline int parse_aaa_avps(char *definition,
										struct aaa_avp **avp_def, int *cnt)
{
	struct aaa_avp *avp;
	int_str avp_name;
	pv_spec_t avp_spec;
	str  foo;
	char *p;
	char *e;
	char *s;
	char t;

	p = definition;
	*avp_def = 0;
	*cnt = 0;

	if (p==0 || *p==0)
		return 0;

	/* get element by element */
	while ( (e=strchr(p,';'))!=0 || (e=p+strlen(p))!=p ) {
		/* new aaa_avp struct */
		if ( (avp=(struct aaa_avp*)pkg_malloc(sizeof(struct aaa_avp)))==0 ) {
			LM_ERR("no more pkg mem\n");
			goto error;
		}
		memset( avp, 0, sizeof(struct aaa_avp));
		/* definition is between p and e */
		if ( (s=strchr(p,'='))!=0 && s<e ) {
			/* avp = attr */
			foo.s = p;
			foo.len = s-p;
			trim( &foo );
			if (foo.len==0)
				goto parse_error;
			t = foo.s[foo.len];
			foo.s[foo.len] = '\0';
			
			if (pv_parse_spec(&foo, &avp_spec)==0
					|| avp_spec.type!=PVT_AVP) {
				LM_ERR("malformed or non AVP %s AVP definition\n", foo.s);
				goto parse_error;;
			}

			if(pv_get_avp_name(0, &(avp_spec.pvp), &avp_name,
						&avp->avp_type)!=0)
			{
				LM_ERR("[%s]- invalid AVP definition\n", foo.s);
				goto parse_error;
			}
			foo.s[foo.len] = t;

			/* copy the avp name into the avp structure */
			if (avp->avp_type&AVP_NAME_STR) {
				avp->avp_name.s.s =
					(char*)pkg_malloc(avp_name.s.len+1);
				if (avp->avp_name.s.s==0) {
					LM_ERR("no more pkg mem\n");
					goto error;
				}
				avp->avp_name.s.len = avp_name.s.len;
				memcpy(avp->avp_name.s.s, avp_name.s.s, avp_name.s.len);
				avp->avp_name.s.s[avp->avp_name.s.len] = 0;
			} else {
				avp->avp_name.n = avp_name.n;
			}
			/* go to after the equal sign */
			p = s+1;
		}
		/* attr - is between p and e*/
		foo.s = p;
		foo.len = e-p;
		trim( &foo );
		if (foo.len==0)
			goto parse_error;
		/* copy the attr into the avp structure */
		avp->attr_name.s = (char*)pkg_malloc( foo.len+1 );
		if (avp->attr_name.s==0) {
			LM_ERR("no more pkg mem\n");
			goto error;
		}
		avp->attr_name.len = foo.len;
		memcpy( avp->attr_name.s, foo.s, foo.len );
		avp->attr_name.s[foo.len] = 0;
		/* was an avp name specified? */
		if (avp->avp_name.s.s==0) {
			/* use as avp_name the attr name */
			avp->avp_type = AVP_NAME_STR;
			avp->avp_name.s = avp->attr_name;
		}
		/* link the element */
		avp->next = *avp_def;
		*avp_def = avp;
		(*cnt)++;
		avp = 0;
		/* go to the end */
		p = e;
		if (*p==';')
			p++;
		if (*p==0)
			break;
	}

	return 0;
parse_error:
	LM_ERR("parse failed in \"%s\" at pos %d(%s)\n",
		definition, (int)(long)(p-definition),p);
error:
	free_aaa_avp( avp );
	free_aaa_avp_list( *avp_def );
	*avp_def = 0;
	*cnt = 0;
	return -1;
}

#endif
