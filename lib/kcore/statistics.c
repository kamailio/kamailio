/*
 * $Id$
 *
 * Copyright (C) 2006 Voice Sistem SRL
 *
 * This file is part of Kamailio, a free SIP server.
 *
 * Kamailio is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * Kamailio is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 59 Temple Place - Suite 330, Boston, MA  02111-1307, USA.
 *
 *
 * History:
 * ---------
 *  2006-01-16  first version (bogdan)
 *  2006-11-28  added get_stat_var_from_num_code() (Jeffrey Magder -
 *              SOMA Networks)
 */

/*!
 * \file
 * \brief Statistics support
 */


#include <string.h>
#include <stdio.h>

#include "../../mem/shm_mem.h"
#include "../kmi/mi.h"
#include "../../ut.h"
#include "../../dprint.h"
#include "../../locking.h"
#include "../../socket_info.h"
#include "core_stats.h"
#include "statistics.h"

#ifdef STATISTICS

#define MAX_PROC_BUFFER 256

static stats_collector *collector;

static struct mi_root *mi_get_stats(struct mi_root *cmd, void *param);
static struct mi_root *mi_reset_stats(struct mi_root *cmd, void *param);

static mi_export_t mi_stat_cmds[] = {
	{ "get_statistics",    mi_get_stats,    0  ,  0,  0 },
	{ "reset_statistics",  mi_reset_stats,  0  ,  0,  0 },
	{ 0, 0, 0, 0, 0}
};



#ifdef NO_ATOMIC_OPS
#warning STATISTICS: Architecture with no support for atomic operations. \
         Using Locks!!
gen_lock_t *stat_lock = 0;
#endif

#define stat_hash(_s) core_hash( _s, 0, STATS_HASH_SIZE)



/*! \brief
 * Returns the statistic associated with 'numerical_code' and 'out_codes'.
 * Specifically:
 *
 *  - if out_codes is nonzero, then the stat_var for the number of messages 
 *    _sent out_ with the 'numerical_code' will be returned if it exists.
 *  - otherwise, the stat_var for the number of messages _received_ with the 
 *    'numerical_code' will be returned, if the stat exists. 
 */
stat_var *get_stat_var_from_num_code(unsigned int numerical_code, int out_codes)
{
	static char msg_code[INT2STR_MAX_LEN+4];
	str stat_name;

	stat_name.s = int2bstr( (unsigned long)numerical_code, msg_code, 
		&stat_name.len);
	stat_name.s[stat_name.len++] = '_';

	if (out_codes) {
		stat_name.s[stat_name.len++] = 'o';
		stat_name.s[stat_name.len++] = 'u';
		stat_name.s[stat_name.len++] = 't';
	} else {
		stat_name.s[stat_name.len++] = 'i';
		stat_name.s[stat_name.len++] = 'n';
	}

	return get_stat(&stat_name);
}



int init_stats_collector(void)
{
	/* init the collector */
	collector = (stats_collector*)shm_malloc(sizeof(stats_collector));
	if (collector==0) {
		LM_ERR("no more shm mem\n");
		goto error;
	}
	memset( collector, 0 , sizeof(stats_collector));

#ifdef NO_ATOMIC_OPS
	/* init BIG (really BIG) lock */
	stat_lock = lock_alloc();
	if (stat_lock==0 || lock_init( stat_lock )==0 ) {
		LM_ERR("failed to init the really BIG lock\n");
		goto error;
	}
#endif

	/* register MI commands */
	if (register_mi_mod( "statistics", mi_stat_cmds)<0) {
		LM_ERR("unable to register MI cmds\n");
		goto error;
	}

	/* register core statistics */
	if (register_module_stats( "core", core_stats)!=0 ) {
		LM_ERR("failed to register core statistics\n");
		goto error;
	}
	/* register sh_mem statistics */
	if (register_module_stats( "shmem", shm_stats)!=0 ) {
		LM_ERR("failed to register sh_mem statistics\n");
		goto error;
	}
	LM_DBG("statistics manager successfully initialized\n");

	return 0;
error:
	return -1;
}


void destroy_stats_collector(void)
{
	stat_var *stat;
	stat_var *tmp_stat;
	int i;

#ifdef NO_ATOMIC_OPS
	/* destroy big lock */
	if (stat_lock)
		lock_destroy( stat_lock );
#endif

	if (collector) {
		/* destroy hash table */
		for( i=0 ; i<STATS_HASH_SIZE ; i++ ) {
			for( stat=collector->hstats[i] ; stat ; ) {
				tmp_stat = stat;
				stat = stat->hnext;
				if ((tmp_stat->flags&STAT_IS_FUNC)==0 && tmp_stat->u.val)
					shm_free(tmp_stat->u.val);
				if ( (tmp_stat->flags&STAT_SHM_NAME) && tmp_stat->name.s)
					shm_free(tmp_stat->name.s);
				shm_free(tmp_stat);
			}
		}

		/* destroy sts_module array */
		if (collector->amodules)
			shm_free(collector->amodules);

		/* destroy the collector */
		shm_free(collector);
	}

	return;
}


static inline module_stats* get_stat_module( str *module)
{
	int i;

	if ( (module==0) || module->s==0 || module->len==0 )
		return 0;

	for( i=0 ; i<collector->mod_no ; i++ ) {
		if ( (collector->amodules[i].name.len == module->len) &&
		(strncasecmp(collector->amodules[i].name.s,module->s,module->len)==0) )
			return &collector->amodules[i];
	}

	return 0;
}


static inline module_stats* add_stat_module( char *module)
{
	module_stats *amods;
	module_stats *mods;
	int len;

	if ( (module==0) || ((len = strlen(module))==0 ) )
		return 0;

	amods = (module_stats*)shm_realloc( collector->amodules,
			(collector->mod_no+1)*sizeof(module_stats) );
	if (amods==0) {
		LM_ERR("no more shm memory\n");
		return 0;
	}

	collector->amodules = amods;
	collector->mod_no++;

	mods = &amods[collector->mod_no-1];
	memset( mods, 0, sizeof(module_stats) );

	mods->name.s = module;
	mods->name.len = len;

	return mods;
}


int register_stat( char *module, char *name, stat_var **pvar, int flags)
{
	module_stats* mods;
	stat_var *stat;
	stat_var *it;
	str smodule;
	int hash;

	if (module==0 || name==0 || pvar==0) {
		LM_ERR("invalid parameters module=%p, name=%p, pvar=%p \n", 
				module, name, pvar);
		goto error;
	}

	stat = (stat_var*)shm_malloc(sizeof(stat_var));
	if (stat==0) {
		LM_ERR("no more shm memory\n");
		goto error;
	}
	memset( stat, 0, sizeof(stat_var));

	if ( (flags&STAT_IS_FUNC)==0 ) {
		stat->u.val = (stat_val*)shm_malloc(sizeof(stat_val));
		if (stat->u.val==0) {
			LM_ERR("no more shm memory\n");
			goto error1;
		}
#ifdef NO_ATOMIC_OPS
		*(stat->u.val) = 0;
#else
		atomic_set(stat->u.val,0);
#endif
		*pvar = stat;
	} else {
		stat->u.f = (stat_function)(pvar);
	}

	/* is the module already recorded? */
	smodule.s = module;
	smodule.len = strlen(module);
	mods = get_stat_module(&smodule);
	if (mods==0) {
		mods = add_stat_module(module);
		if (mods==0) {
			LM_ERR("failed to add new module\n");
			goto error2;
		}
	}

	/* fill the stat record */
	stat->mod_idx = collector->mod_no-1;

	stat->name.s = name;
	stat->name.len = strlen(name);
	stat->flags = flags;


	/* compute the hash by name */
	hash = stat_hash( &stat->name );

	/* link it */
	if (collector->hstats[hash]==0) {
		collector->hstats[hash] = stat;
	} else {
		it = collector->hstats[hash];
		while(it->hnext)
			it = it->hnext;
		it->hnext = stat;
	}
	collector->stats_no++;

	/* add the statistic also to the module statistic list */
	if (mods->tail) {
		mods->tail->lnext = stat;
	} else {
		mods->head = stat;
	}
	mods->tail = stat;
	mods->no++;

	return 0;
error2:
	if ( (flags&STAT_IS_FUNC)==0 ) {
		shm_free(*pvar);
		*pvar = 0;
	}
error1:
	shm_free(stat);
error:
	*pvar = 0;
	return -1;
}



int register_module_stats(char *module, stat_export_t *stats)
{
	int ret;

	if (module==0 || module[0]==0 || !stats || !stats[0].name)
		return 0;

	for( ; stats->name ; stats++) {
		ret = register_stat( module, stats->name, stats->stat_pointer,
			stats->flags);
		if (ret!=0) {
			LM_CRIT("failed to add statistic\n");
			return -1;
		}
	}

	return 0;
}



stat_var* get_stat( str *name )
{
	stat_var *stat;
	int hash;

	if (name==0 || name->s==0 || name->len==0)
		return 0;

	/* compute the hash by name */
	hash = stat_hash( name );

	/* and look for it */
	for( stat=collector->hstats[hash] ; stat ; stat=stat->hnext ) {
		if ( (stat->name.len==name->len) &&
		(strncasecmp( stat->name.s, name->s, name->len)==0) )
			return stat;
	}

	return 0;
}

/*!
 * This function will retrieve a list of all ip addresses and ports that OpenSER
 * is listening on, with respect to the transport protocol specified with
 * 'protocol'. 
 *
 * The first parameter, ipList, is a pointer to a pointer. It will be assigned a
 * new block of memory holding the IP Addresses and ports being listened to with
 * respect to 'protocol'.  The array maps a 2D array into a 1 dimensional space,
 * and is layed out as follows:
 *
 * The first NUM_IP_OCTETS indices will be the IP address, and the next index
 * the port.  So if NUM_IP_OCTETS is equal to 4 and there are two IP addresses
 * found, then:
 *
 *  - ipList[0] will be the first octet of the first ip address
 *  - ipList[3] will be the last octet of the first ip address.
 *  - iplist[4] will be the port of the first ip address
 *  - 
 *  - iplist[5] will be the first octet of the first ip address, 
 *  - and so on.  
 *
 * The function will return the number of sockets which were found.  This can be
 * used to index into ipList.
 *
 * \note This function assigns a block of memory equal to:
 *
 *            returnedValue * (NUM_IP_OCTETS + 1) * sizeof(int);
 *
 *       Therefore it is CRUCIAL that you free ipList when you are done with its
 *       contents, to avoid a nasty memory leak.
 */
int get_socket_list_from_proto(int **ipList, int protocol) {

	struct socket_info  *si;
	struct socket_info** list;

	int num_ip_octets   = 4;
	int numberOfSockets = 0;
	int currentRow      = 0;

	/* I hate to use #ifdefs, but this is necessary because of the way 
	 * get_sock_info_list() is defined.  */
#ifndef USE_TCP
	if (protocol == PROTO_TCP) 
	{
		return 0;
	}
#endif

#ifndef USE_TLS
	if (protocol == PROTO_TLS)
	{
		return 0;
	}
#endif

	/* Retrieve the list of sockets with respect to the given protocol. */
	list=get_sock_info_list(protocol);

	/* Find out how many sockets are in the list.  We need to know this so
	 * we can malloc an array to assign to ipList. */
	for(si=list?*list:0; si; si=si->next){
		/* We only support IPV4 at this point. */
		if (si->address.af == AF_INET) {
			numberOfSockets++;
		}
	}

	/* There are no open sockets with respect to the given protocol. */
	if (numberOfSockets == 0)
	{
		return 0;
	}

	*ipList = pkg_malloc(numberOfSockets * (num_ip_octets + 1) * sizeof(int));

	/* We couldn't allocate memory for the IP List.  So all we can do is
	 * fail. */
	if (*ipList == NULL) {
		LM_ERR("no more pkg memory");
		return 0;
	}


	/* We need to search the list again.  So find the front of the list. */
	list=get_sock_info_list(protocol);

	/* Extract out the IP Addresses and ports.  */
	for(si=list?*list:0; si; si=si->next){

		/* We currently only support IPV4. */
		if (si->address.af != AF_INET) {
			continue;
		}

		(*ipList)[currentRow*(num_ip_octets + 1)  ] = 
			si->address.u.addr[0];
		(*ipList)[currentRow*(num_ip_octets + 1)+1] = 
			si->address.u.addr[1];
		(*ipList)[currentRow*(num_ip_octets + 1)+2] = 
			si->address.u.addr[2];
		(*ipList)[currentRow*(num_ip_octets + 1)+3] = 
			si->address.u.addr[3];
		(*ipList)[currentRow*(num_ip_octets + 1)+4] = 
			si->port_no;
		
		currentRow++;
	}

	return numberOfSockets;
}

/*!
 * Takes a 'line' (from the proc file system), parses out the ipAddress,
 * address, and stores the number of bytes waiting in 'rx_queue'
 *
 * Returns 1 on success, and 0 on a failed parse.
 *
 * Note: The format of ipAddress is as defined in the comments of
 * get_socket_list_from_proto() in this file. 
 *
 */
static int parse_proc_net_line(char *line, int *ipAddress, int *rx_queue) 
{
	int i;

	int ipOctetExtractionMask = 0xFF;

	char *currColonLocation;
	char *nextNonNumericalChar;
	char *currentLocationInLine = line;

	int parsedInteger[4];

	/* Example line from /proc/net/tcp or /proc/net/udp:
	 *
	 *	sl  local_address rem_address   st tx_queue rx_queue  
	 *	21: 5A0A0B0A:CAC7 1C016E0A:0016 01 00000000:00000000
	 *
	 * Algorithm:
	 *
	 * 	1) Find the location of the first  ':'
	 * 	2) Parse out the IP Address into an integer
	 * 	3) Find the location of the second ':'
	 * 	4) Parse out the port number.
	 * 	5) Find the location of the fourth ':'
	 * 	6) Parse out the rx_queue.
	 */

	for (i = 0; i < 4; i++) {

		currColonLocation = strchr(currentLocationInLine, ':'); 

		/* We didn't find all the needed ':', so fail. */
		if (currColonLocation == NULL) {
			return 0;
		}

		/* Parse out the integer, keeping the location of the next 
		 * non-numerical character.  */
		parsedInteger[i] = 
			(int) strtol(++currColonLocation, &nextNonNumericalChar,
					16);

		/* strtol()'s specifications specify that the second parameter
		 * is set to the first parameter when a number couldn't be
		 * parsed out.  This means the parse was unsuccesful.  */
		if (nextNonNumericalChar == currColonLocation) {
			return 0;
		}
		
		/* Reset the currentLocationInLine to the last non-numerical 
		 * character, so that next iteration of this loop, we can find
		 * the next colon location. */
		currentLocationInLine = nextNonNumericalChar;

	}

	/* Extract out the segments of the IP Address.  They are stored in
	 * reverse network byte order. */
	for (i = 0; i < NUM_IP_OCTETS; i++) {
		
		ipAddress[i] = 
			parsedInteger[0] & (ipOctetExtractionMask << i*8); 

		ipAddress[i] >>= i*8;

	}

	ipAddress[NUM_IP_OCTETS] = parsedInteger[1];

	*rx_queue = parsedInteger[3];
	
	return 1;
 
}


/*!
 * Returns 1 if ipOne was found in ipArray, and 0 otherwise. 
 *
 * The format of ipOne and ipArray are described in the comments of 
 * get_socket_list_from_proto() in this file.
 *
 * */
static int match_ip_and_port(int *ipOne, int *ipArray, int sizeOf_ipArray) 
{
	int curIPAddrIdx;
	int curOctetIdx;
	int ipArrayIndex;

	/* Loop over every IP Address */
	for (curIPAddrIdx = 0; curIPAddrIdx < sizeOf_ipArray; curIPAddrIdx++) {

		/* Check for octets that don't match.  If one is found, skip the
		 * rest.  */
		for (curOctetIdx = 0; curOctetIdx < NUM_IP_OCTETS + 1; curOctetIdx++) {
			
			/* We've encoded a 2D array as a 1D array.  So find out
			 * our position in the 1D array. */
			ipArrayIndex = 
				curIPAddrIdx * (NUM_IP_OCTETS + 1) + curOctetIdx;

			if (ipOne[curOctetIdx] != ipArray[ipArrayIndex]) {
				break;
			}
		}

		/* If the index from the inner loop is equal to NUM_IP_OCTETS
		 * + 1, then that means that every octet (and the port with the
		 * + 1) matched. */
		if (curOctetIdx == NUM_IP_OCTETS + 1) {
			return 1;
		}

	}

	return 0;
}


/*!
 * Returns the number of bytes waiting to be consumed on the network interfaces
 * assigned the IP Addresses specified in interfaceList.  The check will be
 * limited to the TCP or UDP transport exclusively.  Specifically:
 *
 * - If forTCP is non-zero, the check involves only the TCP transport.
 * - if forTCP is zero, the check involves only the UDP transport.
 *
 * Note: This only works on linux systems supporting the /proc/net/[tcp|udp]
 *       interface.  On other systems, zero will always be returned. 
 */
static int get_used_waiting_queue(
		int forTCP, int *interfaceList, int listSize) 
{
	FILE *fp;
	char *fileToOpen;
	
	char lineBuffer[MAX_PROC_BUFFER];
	int  ipAddress[NUM_IP_OCTETS+1];
	int  rx_queue;
	
	int  waitingQueueSize = 0;

	/* Set up the file we want to open. */
	if (forTCP) {
		fileToOpen = "/proc/net/tcp";
	} else {
		fileToOpen = "/proc/net/udp";
	}
	
	fp = fopen(fileToOpen, "r");

	if (fp == NULL) {
		LM_ERR("Could not open %s. openserMsgQueu eDepth and its related"
				" alarms will not be available.\n", fileToOpen);
		return 0;
	}

	/* Read in every line of the file, parse out the ip address, port, and
	 * rx_queue, and compare to our list of interfaces we are listening on.
	 * Add up rx_queue for those lines which match our known interfaces. */
	while (fgets(lineBuffer, MAX_PROC_BUFFER, fp)!=NULL) {

		/* Parse out the ip address, port, and rx_queue. */
		if(parse_proc_net_line(lineBuffer, ipAddress, &rx_queue)) {

			/* Only add rx_queue if the line just parsed corresponds 
			 * to an interface we are listening on.  We do this
			 * check because it is possible that this system has
			 * other network interfaces that OpenSER has been told
			 * to ignore. */
			if (match_ip_and_port(ipAddress, interfaceList, listSize)) {
				waitingQueueSize += rx_queue;
			}
		}
	}

	fclose(fp);

	return waitingQueueSize;
}

/*!
 * Returns the sum of the number of bytes waiting to be consumed on all network
 * interfaces and transports that OpenSER is listening on. 
 *
 * Note: This currently only works on systems supporting the /proc/net/[tcp|udp]
 *       interface.  On other systems, zero will always be returned.  To change
 *       this in the future, add an equivalent for get_used_waiting_queue(). 
 */
int get_total_bytes_waiting(void) 
{
	int bytesWaiting = 0;

	int *UDPList  = NULL;
	int *TCPList  = NULL;
	int *TLSList  = NULL;

	int numUDPSockets  = 0;
	int numTCPSockets  = 0; 
	int numTLSSockets  = 0;

	/* Extract out the IP address address for UDP, TCP, and TLS, keeping
	 * track of the number of IP addresses from each transport  */
	numUDPSockets  = get_socket_list_from_proto(&UDPList,  PROTO_UDP);
	numTCPSockets  = get_socket_list_from_proto(&TCPList,  PROTO_TCP);
	numTLSSockets  = get_socket_list_from_proto(&TLSList,  PROTO_TLS);

	/* Find out the number of bytes waiting on our interface list over all
	 * UDP and TCP transports. */
	bytesWaiting  += get_used_waiting_queue(0, UDPList,  numUDPSockets);
	bytesWaiting  += get_used_waiting_queue(1, TCPList,  numTCPSockets);
	bytesWaiting  += get_used_waiting_queue(1, TLSList,  numTLSSockets);

	/* get_socket_list_from_proto() allocated a chunk of memory, so we need
	 * to free it. */
	if (numUDPSockets > 0)
	{
		pkg_free(UDPList);
	}

	if (numTCPSockets > 0) 
	{
		pkg_free(TCPList);
	}

	if (numTLSSockets > 0)
	{
		pkg_free(TLSList);
	}

	return bytesWaiting;
}



/***************************** MI STUFF ********************************/

inline static int mi_add_stat(struct mi_node *rpl, stat_var *stat)
{
	struct mi_node *node;

	node = addf_mi_node_child(rpl, 0, 0, 0, "%.*s:%.*s = %lu",
		collector->amodules[stat->mod_idx].name.len,
		collector->amodules[stat->mod_idx].name.s,
		stat->name.len, stat->name.s,
		get_stat_val(stat) );

	if (node==0)
		return -1;
	return 0;
}

inline static int mi_add_module_stats(struct mi_node *rpl,
													module_stats *mods)
{
	struct mi_node *node;
	stat_var *stat;

	for( stat=mods->head ; stat ; stat=stat->lnext) {
		node = addf_mi_node_child(rpl, 0, 0, 0, "%.*s:%.*s = %lu",
			mods->name.len, mods->name.s,
			stat->name.len, stat->name.s,
			get_stat_val(stat) );
		if (node==0)
			return -1;
	}
	return 0;
}


static struct mi_root *mi_get_stats(struct mi_root *cmd, void *param)
{
	struct mi_root *rpl_tree;
	struct mi_node *rpl;
	struct mi_node *arg;
	module_stats   *mods;
	stat_var       *stat;
	str val;
	int i;

	if (cmd->node.kids==NULL)
		return init_mi_tree( 400, MI_MISSING_PARM_S, MI_MISSING_PARM_LEN);

	rpl_tree = init_mi_tree( 200, MI_OK_S, MI_OK_LEN);
	if (rpl_tree==0)
		return 0;
	rpl = &rpl_tree->node;

	for( arg=cmd->node.kids ; arg ; arg=arg->next) {
		if (arg->value.len==0)
			continue;

		val = arg->value;

		if ( val.len==3 && memcmp(val.s,"all",3)==0) {
			/* add all statistic variables */
			for( i=0 ; i<collector->mod_no ;i++ ) {
				if (mi_add_module_stats( rpl, &collector->amodules[i] )!=0)
					goto error;
			}
		} else if ( val.len>1 && val.s[val.len-1]==':') {
			/* add module statistics */
			val.len--;
			mods = get_stat_module( &val );
			if (mods==0)
				continue;
			if (mi_add_module_stats( rpl, mods )!=0)
				goto error;
		} else {
			/* add only one statistic */
			stat = get_stat( &val );
			if (stat==0)
				continue;
			if (mi_add_stat(rpl,stat)!=0)
				goto error;
		}
	}

	if (rpl->kids==0) {
		free_mi_tree(rpl_tree);
		return init_mi_tree( 404, "Statistics Not Found", 20);
	}

	return rpl_tree;
error:
	free_mi_tree(rpl_tree);
	return 0;
}



static struct mi_root *mi_reset_stats(struct mi_root *cmd, void *param)
{
	struct mi_root *rpl_tree;
	struct mi_node *arg;
	stat_var       *stat;
	int found;

	if (cmd->node.kids==NULL)
		return init_mi_tree( 400, MI_MISSING_PARM_S, MI_MISSING_PARM_LEN);

	rpl_tree = init_mi_tree( 200, MI_OK_S, MI_OK_LEN);
	if (rpl_tree==0)
		return 0;
	found = 0;

	for( arg=cmd->node.kids ; arg ; arg=arg->next) {
		if (arg->value.len==0)
			continue;

		stat = get_stat( &arg->value );
		if (stat==0)
			continue;

		reset_stat( stat );
		found = 1;
	}

	if (!found) {
		free_mi_tree(rpl_tree);
		return init_mi_tree( 404, "Statistics Not Found", 20);
	}

	return rpl_tree;
}


#endif /*STATISTICS*/

