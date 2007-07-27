/*
 * $Id: $
 *
 * OpenSER LDAP Module
 *
 * Copyright (C) 2007 University of North Carolina
 *
 * Original author: Christian Schlatter, cs@unc.edu
 *
 *
 * This file is part of openser, a free SIP server.
 *
 * openser is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version
 *
 * openser is distributed in the hope that it will be useful,
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
 * 2007-02-18: Initial version
 */


#ifndef LDAP_CONNECT_H
#define LDAP_CONNECT_H

#include "../../str.h"
#include "../../dprint.h"

extern int ldap_connect(char* _ld_name);
extern int ldap_disconnect(char* _ld_name);
extern int ldap_reconnect(char* _ld_name);
extern int ldap_get_vendor_version(char** _version);

#endif /* LDAP_CONNECT_H */

