/*
 * $Id$
 *
 * Copyright (C) 2006 iptelorg GmbH
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
 * Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA  02110-1301  USA
 */
#include <stdio.h>
#include <string.h>
#include <getopt.h>
#include <arpa/inet.h>
#include <stdlib.h>
#include <search.h>
#include <assert.h>

#include <xmlrpc-c/base.h>
#include <xmlrpc-c/client.h>

#define NAME "SER mod_pike top console"
#define VERSION "1.0"

// XML elements in result
// they MUST NOT have a number on the tail
// because pike uses it for "row" numbering
static const char * const MAX_HITS  = "max_hits";
static const char * const IP_ADDR   = "ip_addr";
static const char * const LEAF_HITS_PREV = "leaf_hits_prev";
static const char * const LEAF_HITS_CURR = "leaf_hits_curr";
static const char * const EXPIRES   = "expires";
static const char * const STATUS    = "status";
static const char * const NUMBER_OF_ROWS  = "number_of_rows";

#define IP_ADDR_MAX_LENGTH 40
#define STATUS_MAX_LENGTH  10
typedef struct TopItem {
	char             ip_addr[IP_ADDR_MAX_LENGTH];
	in_addr_t        ipv4_addr;           /* uint32_t */
	struct in6_addr  ipv6_addr;
	unsigned short   leaf_hits[2];
	unsigned int     expires;
	char status[STATUS_MAX_LENGTH];
	int  num_of_ips;                           /* number of IP addresses in aggregated result */
} TopItem;

int compare_TopItem_hits(const void* left, const void *right)
{
	TopItem *li = (TopItem *)left;
	TopItem *ri = (TopItem *)right;
	return li->leaf_hits[0] + li->leaf_hits[1] - ri->leaf_hits[0] - ri->leaf_hits[1];
}
/** Compare function to qsort array in reverse order (biger first) */ 
int compare_TopItem_hits_reverse(const void* left, const void *right)
{
	return compare_TopItem_hits(right, left);
}

int compare_TopItem_ipv4_addr(const void *item1, const void *item2)
{
	return ((TopItem*)item1)->ipv4_addr - ((TopItem*)item2)->ipv4_addr;
}

/**
 * @return  concatenated string in newly allocated memory
 */
static char *concat( const char *name, int index )
{
	char *ptr;
	int rv;
	
	rv = asprintf(&ptr, "%s%d", name, index);
	if ( rv == -1 )
		return 0;
	
	return ptr;
}

static void strfree( char *ptr )
{
	if (ptr)
		free(ptr);
}

static void die_if_fault_occurred (xmlrpc_env *env)
{
	if (env->fault_occurred) {
		fprintf(stderr, "XML-RPC Fault: %s (%d)\n",
				env->fault_string, env->fault_code);
		exit(1);
	}
}

/** @return 0 if everything is OK, 1 otherwise */
static int fault_occurred (xmlrpc_env *env)
{
	if (env->fault_occurred) {
		fprintf(stderr, "XML-RPC Fault: %s (%d)\n",
				env->fault_string, env->fault_code);
		return 1;
	}
	return 0;
}

static void die_if_fault_occurred_line (xmlrpc_env *env, int line)
{
	if (env->fault_occurred)
		fprintf(stderr, "LINE: %d\n", line);
	die_if_fault_occurred(env);
}

void print_help()
{
	printf("\n");
	if ( isatty(1) )
		printf("usage: \033[1mpike_top <ser addr>:<port> [--hot|--warm|--all] [--mask]\033[0m\n");
	else
		printf("usage: pike_top <ser addr>:<port> [--hot|--warm|--all] [--mask] [--ipleaf|--inner]\n");
	printf("\n");
	printf("\toptions:\n"
	       "\t\t--hot   ...  show hot IP leaves\n"
	       "\t\t--all   ...  show all IP leaves\n"
	       "\t\t--mask  ...  aggregate results regarding IP mask length\n"
	       "\t\t             (default 32, i.e. not aggregate)\n"
	       "\t\t             IPv4 only at this time\n\n"
	       "\t\t\tdefault is to show HOT nodes\n\n");
	if ( isatty(1) )
		printf("You can use \033[1mwatch\033[0m(1) utility for periodical output.\n\n");
	else
		printf("You can use watch(1) utility for periodical output.\n\n");
/*
	printf("Note:\n"
	       "It is a question if reporting warm nodes is useful and if yes, how to report\n"
	       "them. I feel that should be more welcome to report parents of warm nodes,\n"
		   "or something like this to show which subnets could be problematic...\n\n");
*/
}

/* Following options are conforming to definition of node status defined in pike/ip_tree.h */
#define OPT_WARM    1
#define OPT_HOT     2
#define OPT_ALL     3
/**
 * @param   options   ORed cmdline options
 * @return  position of first non option parameter
 */
int process_options(int argc, char *argv[], int *options, int *mask_length)
{
	static struct option long_options[] = {
			{"hot",   0, 0, 'h'},
/*			{"warm",  0, 0, 'w'}, */
			{"all",   0, 0, 'a'},
			{"mask",  0, 0, 'm'},
			{"help",  0, 0, '?'},
			{0,0,0,0}
	};
	int c, index, counter;
	
	*options = 0;
	counter = 0;
	
	while ( (c=getopt_long(argc, argv, "hwam:", long_options, &index)) != -1 ) {
		switch (c) {
			case 'h':
				*options = OPT_HOT;
				++counter;
				break;
/*			case 'w':
				*options = OPT_WARM;
				++counter;
				break;
*/			case 'a':
				*options = OPT_ALL;
				++counter;
				break;
			case 'm':
				*mask_length = atoi(optarg);
				;
				break;
			case '?':
			default:
				print_help();
				exit(0);
				break;
		}
	}
	if ( counter > 1 ) {
		fprintf(stderr, "ERROR: Node type selectors are exlusive, only one of them can be used\n");
		print_help();
		exit(1);
	}
	if ( *options == 0 )
		*options = OPT_HOT;

	return optind;
}


/* Get an integer value from struct result of xmlrpc_client_call()
 * @param structP pointer to a result struct
 * @param element_name name of structure item
 * @param rv returned value
 * @return 1 if succeed and 0 otherwise
 */
int get_int_from_struct_by_name(xmlrpc_value *structP, const char *element_name, int *rv)
{
	xmlrpc_env env;
	xmlrpc_env_init(&env);
	xmlrpc_value *valueP;
	
	xmlrpc_struct_find_value(&env, structP, element_name, &valueP);
	if ( env.fault_occurred )
		goto error;
	
	xmlrpc_read_int(&env, valueP, rv);
	if ( env.fault_occurred )
		goto error1;
	
	xmlrpc_DECREF(valueP);
	return 1;

error1:
	xmlrpc_DECREF(valueP);
error:
	return 0;	
}
/* Get a new string value from struct result of xmlrpc_client_call()
 * @param structP pointer to a result struct
 * @param element_name name of structure item
 * @param rv contains newly allocated string or NULL if fails
 * @return 1 if succeed and 0 otherwise
 */
/* FIXME terminates the programm if it fails  */
int get_string_from_struct_by_name(xmlrpc_value *structP, const char *element_name, char **rv)
{
	xmlrpc_env env;
	xmlrpc_env_init(&env);
	xmlrpc_value *valueP;
	int length;
	
	xmlrpc_struct_find_value(&env, structP, element_name, &valueP);
	die_if_fault_occurred_line(&env, __LINE__);
	xmlrpc_read_string(&env, valueP, (const char **)rv);
	die_if_fault_occurred_line(&env, __LINE__);
	xmlrpc_DECREF(valueP);
	return 1;
}
/* Get an integer value from struct result of xmlrpc_client_call()
 * @param structP pointer to a result struct
 * @param index   index of requested element
 * @param rv returned value
 * @return 1 if succeed and 0 otherwise
 */
/* FIXME terminates the program if it fails */
int get_int_from_struct_by_idx(xmlrpc_value *structP, int index, int *rv)
{
	xmlrpc_env env;
	xmlrpc_env_init(&env);
	xmlrpc_value *keyP;
	xmlrpc_value *valueP;
	
	xmlrpc_struct_read_member(&env, structP, index, &keyP, &valueP);            /* increment refcount of returned values */
	die_if_fault_occurred_line(&env, __LINE__);
	xmlrpc_read_int(&env, valueP, rv);
	die_if_fault_occurred_line(&env, __LINE__);
	xmlrpc_DECREF(valueP);
	return 1;
}

enum _value_type {
	TYPE_NOT_DEF = 0,
	TYPE_INTEGER = 1,
	TYPE_STRING  = 2
};
typedef enum _value_type value_type;

struct _key_value_pair {
	char *key;
	value_type type;
	union value {
		int integer;
		char *string;
		void *value;
	} value;
};
typedef struct _key_value_pair key_value_pair;
void key_value_pair_cleanup(key_value_pair *kvp)
{
	if (kvp && kvp->key) {
		free(kvp->key);
	}
	if (kvp && kvp->type == TYPE_STRING && kvp->value.string) {
		free(kvp->value.string);
	}
	kvp->key = 0;
	kvp->type = TYPE_NOT_DEF;
	kvp->value.integer = 0;
}

/** Get a string value from struct result of xmlrpc_client_call()
 * @param structP pointer to a result struct
 * @param index   index of requested element
 * @param rv pointer to key_value_pair
 * @return 1 if succeed and 0 otherwise
 */
/* FIXME terminates the programm if it fails  */
int get_struct_item_by_idx(xmlrpc_value *structP, int index, key_value_pair *rv)
{
	xmlrpc_env env;
	xmlrpc_env_init(&env);
	xmlrpc_value *keyP;
	xmlrpc_value *valueP;
	int length;
	const char *string;
	
	xmlrpc_struct_read_member(&env, structP, index, &keyP, &valueP);		/* increment refcount of returned values */
	die_if_fault_occurred_line(&env, __LINE__);
	xmlrpc_read_string(&env, keyP, (const char **)&rv->key);
	/* handle value type */
	switch ( xmlrpc_value_type(valueP) ) {
		case XMLRPC_TYPE_INT:
			xmlrpc_read_int(&env, valueP, &rv->value.integer);
			die_if_fault_occurred_line(&env, __LINE__);
			rv->type = TYPE_INTEGER;
			break;
		case XMLRPC_TYPE_STRING:
			xmlrpc_read_string(&env, valueP, &string);
			printf("get_struct_item_by_idx: ptr = %p, string value = '%s'\n", string, string);
			die_if_fault_occurred_line(&env, __LINE__);
			rv->value.string = (char *)string;
			rv->type = TYPE_STRING;
			break;
		default:
			fprintf(stderr, "Wrong type of return value in key: '%s', exiting...\n", rv->key);
			exit(1);
	}
	xmlrpc_DECREF(keyP);							/* decrement refcount */
	xmlrpc_DECREF(valueP);
	die_if_fault_occurred_line(&env, __LINE__);
	/* FIXME add error handling */

	return 1;
}

/**
 *  Reads one toprow from given structure
 * @param structP
 * @param rownum
 * @param top_item
 */
int read_row(xmlrpc_value *structP, int index, TopItem *top_item)
{
	char *elem;
	char *string = 0;
	
	elem = concat(IP_ADDR, index);
	if ( ! get_string_from_struct_by_name(structP, elem, &string) )
		goto error;
	strncpy(top_item->ip_addr, string, sizeof(top_item->ip_addr));
	free(string);
	string = 0;
	free(elem);

	elem = concat(LEAF_HITS_PREV, index);
	if ( ! get_int_from_struct_by_name(structP, elem, (unsigned int *)&top_item->leaf_hits[0]) )
		goto error;
	free(elem);

	elem = concat(LEAF_HITS_CURR, index);
	if ( ! get_int_from_struct_by_name(structP, elem, (unsigned int *)&top_item->leaf_hits[1]) )
		goto error;
	free(elem);

	elem = concat(EXPIRES, index);
	if ( ! get_int_from_struct_by_name(structP, elem, (unsigned int *)&top_item->expires) )
		goto error;
	free(elem);

	elem = concat(STATUS, index);
	if ( ! get_string_from_struct_by_name(structP, elem, &string) )
		goto error;
	strncpy(top_item->status, string, sizeof(top_item->status));
	free(string);
	free(elem);

	return 1;

error:
	if ( string )
		free(string);

	free(elem);
	return 0;
}

void print_row(TopItem *ti)
{
	char *fmt;

	if ( ! ti ) {
		printf("%-15s %10s %10s %10s %-10s\n", "IP address", "HITS PREV", "HITS CURR", "EXPIRES", "STATUS");
		return;
	}
	
	if ( strlen(ti->ip_addr) > 15 )	// IPv6 addr
		fmt = "%s\n                %10d %10d %10d %-10s\n";

	else
		fmt = "%-15s %10d %10d %10d %-10s\n";

	printf(fmt, ti->ip_addr, ti->leaf_hits[0], ti->leaf_hits[1], ti->expires, ti->status);
}
void print_row_agg(TopItem *ti)		/* IPv4 only */
{
	char *fmt;

	if ( ! ti ) {
		printf("%-15s %10s %10s %5s\n", "IP address", "HITS PREV", "HITS CURR", "COUNT");
		return;
	}
	
	fmt = "%-15s %10d %10d %5d\n";

	printf(fmt, ti->ip_addr, ti->leaf_hits[0], ti->leaf_hits[1], ti->num_of_ips);
}

uint32_t mask( int msklen )
{
	if ( msklen )
		return 0xffffffff ^ ((1 << (32-msklen)) - 1);
	else
		return 0;
}

void print_rows(TopItem *root, int nmemb, int mask_length)
{
	int i;
	void (*print_function)(TopItem *);
	
	if (mask_length == 32)
		print_function = print_row;
	else
		print_function = print_row_agg;
	
	print_function(0);
	for ( i = 0; i < nmemb; ++i, ++root ) {
		print_function(root);
	}
}

int main( int argc, char *argv[] )
{

	xmlrpc_env env;
	xmlrpc_value * resultP;
	xmlrpc_value * keyP;
	xmlrpc_value * valueP;
	int struct_size;
	int i, j;
	size_t length;
	const char * str_key_value;
	xmlrpc_int   int_key_value;
	unsigned int max_hits = 0;
	unsigned int rows = 0;
	int rv;
	char *uri;
	int options;		/* what kind of nodes should be processed */
	int uri_pos;		/* position of first non option argument */
	char stropts[16];
	int  pos = 0;
	int  mask_length = 32;	/* 32 means NO aggregate */

	if (argc-1 < 1) {
		print_help();
		exit(0);
	}
	uri_pos = process_options(argc, argv, &options, &mask_length);
	switch (options) {
		case OPT_HOT:
				sprintf(stropts, "HOT");
				break;
		case OPT_ALL:
				sprintf(stropts, "ALL");
				break;
		case OPT_WARM:
				sprintf(stropts, "WARM");
				break;
	}
	printf("Nodes = %s\n", stropts);
	printf("Mask  = /%d\n", mask_length);

	/* Start up our XML-RPC client library. */
	xmlrpc_client_init(XMLRPC_CLIENT_NO_FLAGS, NAME, VERSION);
	
	/* Initialize our error-handling environment. */
	xmlrpc_env_init(&env);

	/* prototype:
	   xmlrpc_value * xmlrpc_client_call(xmlrpc_env * const envP,
	                                     const char * const server_url, const char * const method_name,
	                                     const char * const format, ...);
	 */
	asprintf(&uri, "http://%s/RPC2", argv[uri_pos]);
	resultP = xmlrpc_client_call(&env, uri,
	                             "pike.top",
	                             "(s)", stropts);
	free(uri);
	die_if_fault_occurred_line(&env, __LINE__);
	
	/* parse returned structure */
	if ( xmlrpc_value_type(resultP) != XMLRPC_TYPE_STRUCT ) {
		printf("unexpected result - should be structure\n");
		xmlrpc_env_clean(&env);
		xmlrpc_client_cleanup();
		exit(1);
	}

	struct_size = xmlrpc_struct_size(&env, resultP);
	die_if_fault_occurred_line(&env, __LINE__);
//	printf("Struct size: %d\n", struct_size);

	if ( ! get_int_from_struct_by_name(resultP, MAX_HITS, &max_hits) ) {
		fprintf(stderr, "ERROR: %s not foung in result\n", MAX_HITS);
		exit (1);
	}
	printf("max_hits = %d\n", max_hits);
	if ( ! get_int_from_struct_by_name(resultP, NUMBER_OF_ROWS, &rows) ) {
		fprintf(stderr, "ERROR: %s not foung in result\n", NUMBER_OF_ROWS);
		exit (1);
	}
	printf("rows = %d\n", rows);
	TopItem top_items[rows];
	TopItem *item;			/* tmp item ptr */
	TopItem *result_items = top_items;	/* if no aggregation use this */
	memset(top_items, 0, sizeof(top_items));

	/* aggregated values */
	
	if ( rows == 0 )
		return 0;
	
	for ( i = 0, item = top_items; i < rows; ++i, ++item ) {
		if ( ! read_row(resultP, i, item) ) {
			fprintf(stderr, "ERROR: while reading row number %d\n", i);
		}
		/* fill in ipv4 addr */
//		printf("item[%d].ip_addr = %s, len = %d\n", i, item->ip_addr, strlen(item->ip_addr));
		rv = inet_pton(AF_INET, item->ip_addr, &item->ipv4_addr);
		if ( rv > 0 ) {
//			printf("IPv4 addr: %x\n", item->ipv4_addr);
		} else {
			fprintf(stderr, "IP conversion failed - not an IPv4 address: '%s'\n", item->ip_addr);	/* conversion failed from any reason */
			printf("item[%d].ipv4_addr = %x\n", i, item->ipv4_addr);
		}
			
	}

	assert( rows > 0 );
	/* if IP mask length is shorter than 32 then aggregate list according to the mask */
	if ( mask_length < 32 ) {
		uint32_t ip_mask = htonl(mask(mask_length));
		
		qsort(top_items, rows, sizeof(TopItem), compare_TopItem_ipv4_addr);		/* sort by IPv4 */

		/* skip items without ipv4 address */
		i = 0;	/* index of non aggregated items */
		while (!top_items[i].ipv4_addr && i < rows ) {
			printf("Skip item[%d] - do not has IPv4 address: %s\n", i, top_items[i].ip_addr);
			memset(&top_items[i], 0, sizeof(TopItem));
			++i;
		}

		j = 0;	/* index of aggregated items */
		if ( i == 0 )
			++i;
		
		top_items[0].ipv4_addr &= ip_mask;
		top_items[0].num_of_ips = 1;
		inet_ntop(AF_INET, &top_items[0].ipv4_addr, top_items[0].ip_addr, sizeof(top_items[0].ip_addr));
		while ( i < rows ) {
			top_items[i].ipv4_addr &= ip_mask;
			
			if ( top_items[j].ipv4_addr == top_items[i].ipv4_addr ) {
				top_items[j].leaf_hits[0] += top_items[i].leaf_hits[0];
				top_items[j].leaf_hits[1] += top_items[i].leaf_hits[1];
				++(top_items[j].num_of_ips);
				++i;
			}
			else {
				++j;
				top_items[j] = top_items[i];
				top_items[j].num_of_ips = 1;
				inet_ntop(AF_INET, &top_items[j].ipv4_addr, top_items[j].ip_addr, sizeof(top_items[j].ip_addr));
				++i;
			}
		}
		rows = j + 1;
	}

	qsort(top_items, rows, sizeof(TopItem), compare_TopItem_hits_reverse);

	print_rows( top_items, rows, mask_length );

	/* Dispose of our result value. */
	xmlrpc_DECREF(resultP);

	/* Clean up our error-handling environment. */
	xmlrpc_env_clean(&env);
	/* Shutdown our XML-RPC client library. */
	xmlrpc_client_cleanup();

	return 0;
}
