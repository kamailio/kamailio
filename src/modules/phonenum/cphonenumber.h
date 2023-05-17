/**
 *
 * Copyright (C) 2017 Daniel-Constantin Mierla (asipto.com)
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

#ifndef _CPHONENUMBER_H_
#define _CPHONENUMBER_H_

#ifdef __cplusplus
extern "C"
{
#endif

	// Phone number details
	typedef struct telnum
	{
		char *number;
		char *normalized;
		char *ltype;
		char *ndesc;
		char *ccname;
		char *error;
		int cctel;
		int valid;
	} telnum_t;

	telnum_t *telnum_new(char *);
	void telnum_free(telnum_t *);

	// test if number is possible
	int telnum_possible(char *number, char *region);
	// parse a number
	telnum_t *telnum_parse(char *number, char *region);
	// get country code for number
	char *telnum_cc(char *number);

#ifdef __cplusplus
}
#endif

#endif
