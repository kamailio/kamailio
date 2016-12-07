/*
 * Sdp mangler module
 *
 * Copyright (C) 2001-2003 FhG Fokus
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
 */

#include "ip_helper.h"
#include <stdlib.h>
#include <string.h>
#include <stdio.h>
#include <ctype.h>
#include <sys/types.h>
#include <netinet/in.h>

/* given an ip and a mask it provides network address */
unsigned int
net_address (unsigned int ip, unsigned int mask)
{
	return (ip & mask);
}

/*
	returns 1 if ip belongs to address/net or 0 if not
*/
int
same_net (unsigned int ip, unsigned int address, unsigned int mask)
{
	return ((address & mask) == (ip & mask));
}


/* Small help func takes ip-address-integer and returns string
   representation */
void
ip2str (unsigned int address, char **rr)
{
	int i;
	char *hlp, hlp2[5];	/* initial era hlp2[18] */
	unsigned char *addrp = (unsigned char *) &address;
	hlp = (char *) malloc (18);
	hlp[0] = '\0';

	for (i = 0; i < 3; i++)
	{
		sprintf (hlp2, "%i.", addrp[i]);
		hlp = strcat (hlp, (char *) hlp2);
	}
	sprintf (hlp2, "%i", addrp[3]);
	hlp = strcat (hlp, hlp2);
	*rr = hlp;
}

/* Small help func takes ip-address-string, determines its validity
   and write the integer representation at address.
   Returns 1 if successful converted, 0 if the dotted isn't valid.
   If you want to parse IP/netmask pairs, call parse_ip_netmask
   first - it will remove the netmask, then use this func */
int
parse_ip_address (char *c, unsigned int *address)	//inet_aton
{
	int quat, i, j, digit_bol;
	char buf[20];
	char *p, *q, *r;
	unsigned char *addrp;

	if (c == NULL)
		return 0;
	if (strlen (c) >= 16)
		return 0;

	quat = 0;
	digit_bol = 1;
	buf[0] = '\0';
	/* cool dirty hack to address the bytes of the int easily */
	addrp = (unsigned char *) address;

	/* make a copy of the dotted string, because we modify it */
	strncpy (buf, c, 20);
	p = buf;

	/* search three times for a dot in the string */
	for (i = 0; i < 3; i++)
	{
		if ((q = strchr (p, '.')) == NULL)
			return 0;
		else
		{
			*q = '\0';	/* cut off at the dot */
			if (strlen (p))	/* is the distance between dots greater 0 */
			{
				r = p;
				for (j = 0; j < strlen (p); j++, r++)	/* are all char of the
									 * byte digits */
					digit_bol = digit_bol && isdigit ((unsigned char)*r);
				if (digit_bol)
				{
					quat = atoi (p);
					if (quat > 255)	/* is it a byte or greater */
						return 0;
					else
						addrp[i] =
							(unsigned char) quat;
				}
				else
					return 0;
			}
			else
				return 0;
		}
		p = q + 1;
	}			/* for */

	/* and the last byte */
	if (strlen (p))
	{
		r = p;
		for (j = 0; j < strlen (p); j++, r++)
			digit_bol = digit_bol && isdigit ((unsigned char)*r);
		if (digit_bol)
		{
			quat = atoi (p);
			if (quat > 255)
				{
				return 0;
				}
			else
				addrp[3] = (unsigned char) quat;
			return 1;
		}
		else
			{
			return 0;
			}
	}
	else
		{
		return 0;
		}
}

/* return 1 if ALL str is a positive number or 0. no + signs allowed*/
int
is_positive_number (char *str)
{
	int i;
	if (str == NULL)
		return 0;
	for (i = 0; i < strlen (str); i++)
	{
		if (isdigit ((unsigned char)str[i]) == 0)
			return 0;
	}
	return 1;
}

/* return 0 in case of error */
unsigned int
make_mask (unsigned int length)
{
	unsigned int res;
	if ((length < 8) || (length > 30))
		return -1;	/* invalid value for mask */
	/* fill it with 1 */
	res = 0xFFFFFFFF;
	/* shift it to right with 32-length positions */
	res = htonl (res << (32 - length));
	return res;
}

/* Small help func takes ip-address-string, determines if a valid
   netmask is specified and inserts the netmask into mask.
   Cuts of the netmask of the string, if it founds a netmask !!!
   Returns 0 if no netmask found, -1 if netmask isn't valid, and
   1 if successful.  
	According to this function a mask is in form of 255.255.192.0
	so an ip/mask looks like 10.0.0.0/255.255.192.0
	we will extend it to 10.0.0.0/18 which will be also valid
*/
int
parse_ip_netmask (char *c, char **ip, unsigned int *mask)
{
	char *p, *q;
	unsigned int netmask;

	if (c == NULL)
	{
		return -10;
	}
	p = c;


	if ((q = strchr (p, '/')) == NULL)
	{
		*mask = 0xFFFFFFFF;
		return 0;	/* no mask */
	}
	else
	{
		*ip = (char *) malloc (q - p + 1);
		if ((*ip) == NULL)
			return -2;
		memcpy (*ip, p, q - p);
		(*ip)[q - p] = 0;

		// wrong (*q) = 0;                            /* cut of the netmask */
		q++;
		/*
		 * two possibilities /16 or /255.255.192.0
		 */
		if (is_positive_number (q) == 1)
		{
			/* we have a /16 mask */
			if ((netmask = make_mask (atoi (q))) == 0)
			{
				*mask = 0;	/* error in value of /43 or something like */
				return -1;
			}
			else
			{
				*mask = netmask;
				return 1;
			}
		}
		else /* we may have a 255.255.192.0 mask */ if (parse_ip_address (q, &netmask) == 1)	/* and parse the netmask */
		{
			*mask = netmask;
			return 1;
		}
		else
		{
			*mask = 0;
			return -1;
		}
	}
}

