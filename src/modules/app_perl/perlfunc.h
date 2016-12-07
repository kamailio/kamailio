/*
 * $Id$
 *
 * Perl module for Kamailio
 *
 * Copyright (C) 2006 Collax GmbH
 *                    (Bastian Friedrich <bastian.friedrich@collax.com>)
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
 * Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA  02110-1301  USA
 *
 */

#ifndef PERL_FUNC_H
#define PERL_FUNC_H

#include "../../parser/msg_parser.h"

/*
 * Run a perl function without a sip message parameter.
 */
int perl_exec_simple1(struct sip_msg* _msg, char* fnc, char* str2);
int perl_exec_simple2(struct sip_msg* _msg, char* fnc, char* str2);

/*
 * Run function with a reference to the current SIP message.
 * An optional string may be passed to perl_exec_string.
 */
int perl_exec1(struct sip_msg* _msg, char* fnc, char *foobar);
int perl_exec2(struct sip_msg* _msg, char* fnc, char* mystr);

int app_perl_reset_interpreter(void);

#endif /* PERL_FUNC_H */
