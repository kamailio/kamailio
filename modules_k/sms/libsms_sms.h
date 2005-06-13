/*
 * $Id$
 *
 * Copyright (C) 2001-2003 FhG Fokus
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
 */


#ifndef _LIBSMS_SMS_H
#define _LIBSMS_SMS_H

#include "sms_funcs.h"

#define MAX_MEM  0
#define USED_MEM 1


int putsms( struct sms_msg *sms_messg, struct modem *mdm);

int getsms( struct incame_sms *sms, struct modem *mdm, int sim);

int check_memory( struct modem *mdm, int flag);

void swapchars(char* string, int len);

int cds2sms(struct incame_sms *sms, struct modem *mdm, char *s, int s_len);

#endif

