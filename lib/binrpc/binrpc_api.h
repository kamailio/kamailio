/*
 * Copyright (C) 2006 iptelorg GmbH
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

#ifndef BINRPC_API_H_
#define BINRPC_API_H_

#include "../../modules/ctl/binrpc.h"

struct binrpc_handle {
	int socket;
	int proto;
	int sock_type;
	unsigned char* buf;
	int buf_size;
};

struct binrpc_response_handle {
	unsigned char *reply_buf;
	struct binrpc_parse_ctx in_pkt;
};

/**
 * Function: binrpc_open_connection
 * 
 * Description:
 *   The function open_connection ensures opening of appropriate device for 
 *   future communication. It can create unix socket or TCP/UDP connection 
 *   depending on input parameteres.
 * 
 * @param handle [in]: handler that will be used for saving of obtained socket;
 *   if this function succeed, this handler must be freed via calling of 
 *   binrpc_close_connection function
 * @param name [in]: host IP address or FQDN or unix socket name
 * @param port [in]: host port; in case of unix socket the value is omitted
 * @param proto [in]: type of communication protocol; allowed values are 
 *   UDP_SOCK, TCP_SOCK, UNIXS_SOCK, UNIXD_SOCK
 * @param reply_socket [in]: force reply socket name, for the unix datagram 
 *   socket mode
 * @param sock_dir [in]: specify directory where the reply socket will be 
 *   created; if set to NULL, the default value will be used (/tmp)
 * 
 * @return 0 on success, -1 on failure.
 * 
 * */ 
int binrpc_open_connection(
	struct binrpc_handle* handle,
	char* name, int port, int proto,
	char* reply_socket, char* sock_dir);

/**
 * Function: binrpc_open_connection_url
 * 
 * Description:
 *   The function is similar as open_connection but target is specified using url.
 * 
 * @param handle [in]: handler that will be used for saving of obtained socket;
 *   if this function succeed, this handle must be freed via calling of 
 *   binrpc_close_connection function
 * @param url [in]: [tcp|udp|unix|unixs|unixd] ":" host_socket ":" [port | reply_socket]
 *   Note: unix = unixs
 * 
 * @return 0 on success, -1 on failure.
 * 
 * */ 

int binrpc_open_connection_url(struct binrpc_handle* handle, char* url);
                    
/**
 * Function: binrpc_close_connection
 * 
 * Description:
 *   The function close_connection ensures freeing of active socket that is 
 *   represent by handler
 *
 * @param handle [in]: active connection descriptor
 * 
 * @return 0 on success, -1 on failure.
 *   none
 * */
void binrpc_close_connection(struct binrpc_handle* handle);

/**
 * Function: binrpc_send_command
 * 
 * Description:
 *   The function send_command provides interface for communication with server 
 *   application via binary rpc protocol. It sends request over unix socket or 
 *   TCP/UDPto the host and reads a respons.
 * 
 * @param handle [in]: a descriptor of connection
 * @param method [in]: string value of XMLRPC method (e.g. system.listMethods)
 * @param args [in]: two dimension array of method's attributes
 * @param arg_count [in]: number of method's attributes
 * @param resp_handle [out]: structure for holding binary form of response, must be deallocated using binrpc_release_response
 * 
 * @return 0 on success, -1 on failure. 
 * 
 * */
int binrpc_send_command(
	struct binrpc_handle* handle, 
	char* method, char** args, int arg_count,
	struct binrpc_response_handle* resp_handle);

/**
 * Function: binrpc_send_command_ex
 *
 * Description:
 *   The function send_command_ex is equivalent of send_command and in addition
 *   provides possibility to pass already prepared input values.
 *
 * @param handle [in]: a descriptor of connection
 * @param pkt [in]: packet to be sent
 * @param resp_handle [out]: structure for holding binary form of response, must be deallocated using binrpc_release_response
 *
 * @return 0 on success, -1 on failure.
 *
 * */
int binrpc_send_command_ex(
	struct binrpc_handle* handle, struct binrpc_pkt* pkt,
	struct binrpc_response_handle *resp_handle);

/**
 * Function: binrpc_release_response
 *
 * Description:
 *   The function releases response handle created in binrpc_send_command
 *
 * @param resp_handle [in]: structure for holding binary form of response to be released
 *
 * @return 0 on success, -1 on failure.
 *
 * */
void binrpc_release_response(
	struct binrpc_response_handle *resp_handle
);

/**
 * Function: binrpc_get_response_type
 * 
 * Description:
 *   The function get_response_type provides information about type of response.
 * 
 * @return 1 on valid failure response, 0 on valid successfull, -1 on failure.
 * 
 * */                
int binrpc_get_response_type(struct binrpc_response_handle *resp_handle);

/* 
 * Function: binrpc_parse_response
 * 
 * Description:
 *   parse the body into a malloc allocated, binrpc_val array. Ensure that caller and callee are using the same structure alignment!
 * 
 * @param vals [out]: array of values allocated via (binrpc)malloc; it must be freed "manually"
 * @param val_count [in/out]: number of records in a list
 * @param resp_handle [in]: structure for holding binary form of response
 * 
 * @return -1 failure.
 * 
 * */
int binrpc_parse_response(
	struct binrpc_val** vals,
	int* val_count,
	struct binrpc_response_handle *resp_handle
);
					
/* 
 * Function: binrpc_parse_error_response
 * 
 * Description:
 *   parse the error response
 * 
 * @param resp_handle [in]: structure for holding binary form of response
 * @param err_no [out]: error code
 * @param err [out]: error stringt
 * 
 * @return -1 failure.
 * 
 * */
int binrpc_parse_error_response(
	struct binrpc_response_handle *resp_handle,
	int *err_no,
	char **err
);

/**
 * Function: binrpc_print_response
 * 
 * Description:
 *   The function print_response prints binrpc response to the standard output in 
 *   readable format.
 * 
 * @param resp_handle [in]: structure for holding binary form of response
 * @param fmt [in]: output format that will be used during printing response to 
 *   the standard output
 * 
 * @return 0 on success, -1 on failure
 *  
 * */ 
int binrpc_print_response(struct binrpc_response_handle *resp_handle, char* fmt);

/**
 * Function: binrpc_response_to_text
 * 
 * Description: 
 *   The function binrpc_response_to_text provides functionality to convert result from 
 *   binary form into null terminated text buffer. Records from response are 
 *   separated with character provided in parameter delimiter.
 * 
 * @param resp_handle [in]: structure for holding binary form of response
 * @param txt_rsp [out]: buffer that will be used for text form of result; it 
 *   can be passed into function as a NULL and in this case the function will 
 *   alloc some memory that must be freed by user! This function can also 
 *   realloc txt_rsp buffer if there is not enough space for whole response
 * @param txt_rsp_len [in/out]: this parameter specify number of allocated but 
 *   empty characters in txt_rsp buffer; value of this parameter can be modify
 *   in case that the reallocation will be necessary
 * @param delimiter [in]: a character that will be used for separation of  
 *    records; if no value is provided, the character for new line is used ('\n')
 *
 * @return 0 on success, -1 on failure
 * 
 * */
int binrpc_response_to_text(
	struct binrpc_response_handle *resp_handle, 
        unsigned char** txt_rsp, int* txt_rsp_len, 
	char delimiter);

/**
 * Function: binrpc_set_mallocs
 * 
 * Description: 
 *   The function binrpc_set_mallocs allows to programmer use its own function 
 *   for memory handling.
 * 
 * @param _malloc [in]:   pointer to function that ensures memory allocation
 * @param _realloc [in]:  pointer to function that ensures memory reallocation
 * @param _free [in]:     pointer to function that ensures memory deallocation
 * 
 * */
void binrpc_set_mallocs(void* _malloc, void* _realloc, void* _free);

/**
 * Function: binrpc_get_last_errs
 * 
 * Description: 
 *   The function returns last error that occured when function returned FATAL_ERROR
 * 
 * */
char *binrpc_get_last_errs();

/**
 * Function: binrpc_clear_last_err
 *
 * Description:
 *   The function clears binrpc_last_errs buffer
 *
 * */
void binrpc_clear_last_err();

/**
 * Function: binrpc_free_rpc_array
 *
 * Description:
 *    The function frees memory allocated internally to store reply values
 *    and finally frees the values array
 *
 * */
void binrpc_free_rpc_array(struct binrpc_val* a, int size);

#endif /*BINRPC_API_H_*/
