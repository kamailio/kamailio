/*
 * $Id$
 *
 *
 * Copyright (C) 2001-2003 FhG Fokus
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

#ifndef _H_EXTCMD_CLIENTS
#define _H_EXTCMD_CLIENTS

#define MAX_CLIENTS 2

typedef struct client_struct
{
	int fd;
}client_t;

int nr_clients = 0;
client_t clients[MAX_CLIENTS];



inline int add_client( int fd )
{
	if (nr_clients+1>MAX_CLIENTS)
		return -1;
	clients[nr_clients].fd = fd;
	nr_clients++;
	return 1;
}



inline int del_client( int index )
{
	if (index>=nr_clients || !nr_clients)
		return -1;
	if (nr_clients>1 && nr_clients!=index+1) {
		clients[index].fd = clients[nr_clients-1].fd;
	}
	nr_clients--;
	return 1;
}



inline client_t* get_client( int index )
{
	if (index>=nr_clients)
		return 0;
	return &(clients[index]);
}



inline int get_nr_clients()
{
	return nr_clients;
}



inline int clients_is_full()
{
	return (nr_clients==MAX_CLIENTS);
}



#endif
