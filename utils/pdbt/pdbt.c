/*
 * Copyright (C) 2009 1&1 Internet AG
 *
 * This file is part of sip-router, a free SIP server.
 *
 * sip-router is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version
 *
 * sip-router is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License 
 * along with this program; if not, write to the Free Software 
 * Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA  02110-1301  USA
 */

#define _GNU_SOURCE
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <string.h>
#include <fcntl.h>
#include <sys/time.h>
#include <poll.h>
#include <ctype.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <netdb.h>
#include <errno.h>
#include <limits.h>
#include "dt.h"
#include "dtm.h"
#include "carrier.h"
#include "log.h"




#define NETBUFSIZE 200




typedef void (*query_func_t)(char *number, char *comment, void *data);




void print_usage(char *program) {
	set_log_level(LOG_INFO);
	LINFO("Usage: %s [<option>...] <command> [<param>...]\n", program);
	LINFO("  %s -s <csv file> -m <mmap file> [-k <ids>] [-o] [-u <tree file>] [-l <log level>] build\n", program);
	LINFO("  %s (-m <mmap file>|-r <host>:<port>) [-q <timeout>] [-f <query file>] [-t <carrier text file>] [-l <log level>] query <number>...\n", program);
	LINFO("\n");
	LINFO("  Commands:\n");
	LINFO("    build: Build a mmap image from a csv list.\n");
	LINFO("    query: Query a mmap image or pdb_server.\n");
	LINFO("           Uses Numbers given on commandline or in a file (-f).\n");
	LINFO("\n");
	LINFO("  Options:\n");
	LINFO("    -s <file>: Specifies the csv list.\n");
	LINFO("               Use '-' as filename to read from stdin.\n");
	LINFO("               Line format: <number prefix>;<carrier id>\n");
	LINFO("               Format of carrier id: [0-9][0-9][0-9]\n");
	LINFO("    -m <file>: Specifies the mmap image.\n");
	LINFO("    -f <file>: Specifies the query file.\n");
	LINFO("               Use '-' as filename to read from stdin.\n");
	LINFO("               Each number must be in a separate line.\n");
	LINFO("               Numbers on the command line will not be processed.\n");
	LINFO("    -t <file>: Specifies the file containing carrier names.\n");
	LINFO("               In addition to the carrier code these names will be shown\n");
	LINFO("               when querying numbers.\n");
	LINFO("               Each carrier id and name must be in a separate line.\n");
	LINFO("               Format: D[0-9][0-9][0-9] <name>\n");
	LINFO("    -k <ids>: Keep these carrier ids.\n");
	LINFO("              Merge all other carrier ids into a new id.\n");
	LINFO("              This will save some memory.\n");
	LINFO("              Format: <id>[,<id>...]\n");
	LINFO("    -r <host>:<port>: Host and port to be used for remote server queries.\n");
	LINFO("    -q <timeout>: Timeout for remote server queries in milliseconds.\n");
	LINFO("                  Default is 500 ms.\n");
	LINFO("    -o: Try to optimize the data structure when building a mmap image.\n");
	LINFO("    -u: Write (possibly optimized) tree structure in human-readable format to the given file.\n");
	LINFO("    -l <debug level>: %ld for debug level.\n", LOG_DEBUG);
	LINFO("                      %ld for info level.\n", LOG_INFO);
	LINFO("                      %ld for notice level.\n", LOG_NOTICE);
	LINFO("                      %ld for warning level.\n", LOG_WARNING);
	LINFO("                      %ld for error level.\n", LOG_ERR);
	LINFO("                      %ld for critical level.\n", LOG_CRIT);
	LINFO("                      %ld for alert level.\n", LOG_ALERT);
	LINFO("                      %ld for emergency level.\n", LOG_EMERG);
	LINFO("                      %ld to disable all messages.\n", LOG_EMERG-1);
	LINFO("                      Default is info level.\n");
	LINFO("    -h: Print this help.\n");
}




void print_stats(struct dt_node_t *root) {
	int s;
	int l;
	int c;

	LINFO("+----------------------------------------\n");
	s = dt_size(root);
	LINFO("| %ld nodes in tree (%ld bytes, %ld KB, %ld MB)\n", (long int)s, (long int)s*sizeof(struct dt_node_t), (long int)s*sizeof(struct dt_node_t)/1024, (long int)s*sizeof(struct dt_node_t)/1024/1024);
	l = dt_leaves(root);
	LINFO("| %ld nodes are leaves (%ld bytes, %ld KB, %ld MB)\n", (long int)l, (long int)l*sizeof(struct dt_node_t), (long int)l*sizeof(struct dt_node_t)/1024, (long int)l*sizeof(struct dt_node_t)/1024/1024);
	c = dt_loaded_nodes(root);
	LINFO("| %ld carrier nodes in tree\n", (long int)c);
	LINFO("| \n");
	LINFO("| After saving with leaf node compression:\n");
	LINFO("| %ld nodes in tree (%ld bytes, %ld KB, %ld MB)\n", (long int)s-l, (long int)(s-l)*sizeof(struct dtm_node_t), (long int)(s-l)*sizeof(struct dtm_node_t)/1024, (long int)(s-l)*sizeof(struct dtm_node_t)/1024/1024);
	LINFO("+----------------------------------------\n");
}




int file_query(char *filename, query_func_t query_func, void *data) {
	char * p;
	char * comment;
	FILE * fp;
	char * line = NULL;
	size_t len = 0;
	ssize_t read;

	LINFO("\nprocessing query file '%s'...\n", filename);
  if (strcmp(filename, "-")==0) fp=stdin;
	else fp = fopen(filename, "r");
	if (fp == NULL) {
		LERR("cannot open file '%s'\n", filename);
		return -1;
	}
	while ((read = getline(&line, &len, fp)) != -1) {
		p=line;
		while ((*p >= '0') && (*p <= '9') && (p < line+len)) p++;
		*p='\0';
		p++;
		comment=p;
		while ((*p >= 32) && (p < line+len)) p++;
		*p='\0';
		query_func(line, comment, data);
	}
	if (line) free(line);
	fclose(fp);
	return 0;
}




/*
 Read a csv list from the given file and build a dtree structure.
 Format of lines in csv file: "<number prefix>;<carrier id>".
 Format of carrier id: "[0-9][0-9][0-9]".
 Returns the number of lines imported or -1 on error.
*/
int import_csv(struct dt_node_t *root, char *filename) {
	char *prefix;
	char *carrier_str;
	carrier_t carrier;
	long int ret;

	FILE * fp;
	char * line = NULL;
	size_t len = 0;
	ssize_t read;
	int i=0;
	int n=1;

  if (strcmp(filename, "-")==0) fp=stdin;
	else fp = fopen(filename, "r");
	if (fp == NULL) {
		LERR("cannot open file '%s'\n", filename);
		return -1;
	}
	while ((read = getline(&line, &len, fp)) != -1) {
		carrier_str=line;
		prefix=strsep(&carrier_str, ";");
		ret=strtol(carrier_str, NULL, 10);
		if (!IS_VALID_PDB_CARRIERID(ret)) {
			LWARNING("invalid carrier '%s' in line %ld.\n", carrier_str, (long int)n);
			if (line) free(line);
			fclose(fp);
			return -1;
		}
		else {
			carrier=ret;
			i++;
			dt_insert(root, prefix, strlen(prefix), carrier);
		}
		n++;
	}
	if (line) free(line);
	fclose(fp);
	return i;
}




/*
 Returns 1 if the given node is a leaf node, 0 otherwise.
*/
inline int dt_is_leaf(struct dt_node_t *root)
{
	int i;

	for (i=0; i<10; i++) {
		if (root->child[i]) return 0;
	}

	return 1;
}



/*
 Recursively writes sequences of digits (i.e., telephone numbers/prefixes) and mapped
 carrier ids to the given file descriptor for the entire subtree starting at the
 given node. Each written line matches one such sequence and its mapped carried id.
 Returns 1 on success, -1 otherwise.
 */
int dt_write_tree_recursor(const struct dt_node_t *node, const int fd, char* number)
{
	int i;
	int ret;
	int slen;
	char *buf;
	char *p;
	int bufsize;

	if (node == NULL) return 0;

	slen = strlen(number);
	if (slen > 0) {
		
		bufsize = slen + 1 + 1 + 3 + 1 + 1;		    // line buffer (telephone number + colon + white space + carrier ID + newline + \0)
		buf = (char *)malloc(bufsize);	    
		if (buf == NULL) {
			LERR("could not allocate line output buffer of size %d\n", bufsize);
			return -1;
		}

		/* construct outline line */
		p = strncpy(buf, number, slen);
		p += slen;
		strncpy(p, ": ", 2);
		p += 2;
		ret = snprintf(p, 5, "%d\n", node->carrier);
		if (ret < 1 || ret > 4) {
			LERR("snprintf failed to write correct number of characters\n");
			return -1;
		}

		/* write line to file */
		ret = write(fd, (void *)buf, strlen(buf));
		if (ret != strlen(buf)) {
			LERR("could not write (complete) line output '%s' to file\n", number);
			return -1;
		}
		free(buf);
	}

	for (i=0;i<10;i++) {
		/* extend number by single digit and adjust terminating null byte */
		number[slen] = i + '0';
		number[slen+1] = '\0';	    /* must always be done because other recursive invocations operate on `number' too */
		ret = dt_write_tree_recursor(node->child[i], fd, number);
		if (ret < 0) {
			LERR("could not write node\n");
			return -1;
		}
	}

	return 1;
}



/*
 Writes tree to a file in human-readable format, i.e., ASCII.
 Returns 1 on success, -1 otherwise.
 */
int dt_write_tree(const struct dt_node_t *root, const char* filename)
{
	int fd;
	char number[25];
	number[0] = '\0';

	fd = creat(filename, S_IRWXU);
	if (fd < 0) {
		LERR("cannot create file '%s'\n", filename);
		return -1;
	}
	
	if (dt_write_tree_recursor(root, fd, (char *)&number) < 0) {
		LERR("writing tree to file '%s' failed\n", filename);
		return -1;
	}

	close(fd);
	return 1;
}



/*
  Saves the given node and all sub-nodes to an mmappable file recursively.
  Pointers from parent to child nodes will be represented by positive numbers indexing
  file offsets. The "pointee" address within the file will be linearly proportional to
  this number.
  Pointers to leaf nodes will be replaced by leaf node carrier ids encoded in negative
  numbers, thereby discarding the leaves layer in the tree. Note that this optimization
  accounts for the majority of space saving.
  Returns the index of the next free node, -1 on error.
 */
dtm_node_index_t save_recursor(struct dt_node_t *root, int fd, dtm_node_index_t n) {
	dtm_node_index_t i;
	dtm_node_index_t nn=n+1; /* next free node */
	struct dtm_node_t node;
	int offset;

	node.carrier=root->carrier;
	for (i=0; i<10; i++) {
		if (root->child[i]) {
			if (dt_is_leaf(root->child[i])) {
				node.child[i]=-root->child[i]->carrier;
			}
			else {
				node.child[i]=nn;
				nn=save_recursor(root->child[i], fd, nn);
			}
		}
		else {
			node.child[i]=NULL_CARRIERID;
		}
	}

	offset=lseek(fd, n*sizeof(struct dtm_node_t), SEEK_SET);
	if (offset < 0) {
		LERR("could not position file offset to address %d: errno=%d (%s)\n", n*sizeof(struct dtm_node_t), errno, strerror(errno));
		return -1;
	}
	if (write(fd, &node, sizeof(struct dtm_node_t)) != sizeof(struct dtm_node_t)) {
		LERR("could not write %d bytes of node data at file address %d: errno=%d (%s)\n", sizeof(struct dtm_node_t), offset, errno, strerror(errno));
		return -1;
	}

	return nn;
}




/*
	Saves the given tree in a mmappable file.
	Returns the number of nodes saved or -1 on error.
*/
int save_mmap(struct dt_node_t *root, char *filename) {
	int fd;
	int n;

	fd = open(filename, O_RDWR|O_CREAT|O_TRUNC, S_IRWXU);
	if (fd < 0) {
		LERR("cannot create file '%s'\n", filename);
		return -1;
	}

	n=save_recursor(root, fd, 0);

	close(fd);

	return n;
}




/*
 Returns 1 if carrier is found in keep_carriers, 0 otherwise.
*/
int keep_carrier_func(carrier_t carrier, int keep_carriers_num, carrier_t keep_carriers[])
{
	int i;

	for (i=0; i<keep_carriers_num; i++) {
		if (keep_carriers[i]==carrier) return 1;
	}

	return 0;
}




int merge_carrier_recursor(struct dt_node_t *node, int keep_carriers_num, carrier_t keep_carriers[], carrier_t lastcarrier)
{
  carrier_t currentcarrier;
	int i;
	int sum=0;

	if (node==NULL) return 0;

	if (node->carrier>0) {
		if (!keep_carrier_func(node->carrier, keep_carriers_num, keep_carriers)) {
			sum++;
			if (lastcarrier==0) node->carrier=0; /* first carrier we encountered. we can remove it since we are not interested in it. */
			else {
				node->carrier=OTHER_CARRIERID; /* we already have a carrier we are interested in. this is an exception, set it to a special carrier id. */
			}
		}
	}

	if (node->carrier>0) currentcarrier=node->carrier;
	else currentcarrier=lastcarrier;

	/* merge children carriers */
	for (i=0; i<10; i++) {
		sum+=merge_carrier_recursor(node->child[i], keep_carriers_num, keep_carriers, currentcarrier);
	}

	return sum;
}




/*
 Merge all carriers not in keep_carriers into one new carrier id.
 This will save some memory.
 Returns the number of nodes modified.
*/
int merge_carrier(struct dt_node_t *root, int keep_carriers_num, carrier_t keep_carriers[])
{
	return merge_carrier_recursor(root, keep_carriers_num, keep_carriers, 0);
}




/**
 * return the corresponding carrier id, -1 on error
 */
int query_udp(char *number, int timeout, struct pollfd *pfds, struct sockaddr_in *dstaddr, socklen_t dstaddrlen)
{
	struct timeval tstart, tnow;
	short int carrierid;
	char buf[NETBUFSIZE+1+sizeof(carrierid)];
	size_t reqlen;
	int ret, nflush;
	long int td;

	if (gettimeofday(&tstart, NULL) != 0) {
		LERR("gettimeofday() failed with errno=%d (%s)\n", errno, strerror(errno));
		return -1;
	}

	/* clear recv buffer */
	nflush = 0;
	while (recv(pfds->fd, buf, NETBUFSIZE, MSG_DONTWAIT) > 0) {
		nflush++;
		if (gettimeofday(&tnow, NULL) != 0) {
			LERR("gettimeofday() failed with errno=%d (%s)\n", errno, strerror(errno));
			return -1;
		}
		td=(tnow.tv_usec-tstart.tv_usec+(tnow.tv_sec-tstart.tv_sec)*1000000) / 1000;
		if (td > timeout) {
			LWARNING("exceeded timeout while flushing recv buffer.\n");
			return -1;
		}
	}
	
	/* prepare request */
	reqlen = strlen(number) + 1; /* include null termination */
	if (reqlen > NETBUFSIZE) {
		LERR("number too long '%s'.\n", number);
		return -1;
	}
	strcpy(buf, number);

	/* send request to all servers */
	ret=sendto(pfds->fd, buf, reqlen, MSG_DONTWAIT, (struct sockaddr *)dstaddr, dstaddrlen);
	if (ret < 0) {
		LERR("sendto() failed with errno=%d (%s)\n", errno, strerror(errno));
		return -1;
	}
		
	/* wait for response */
	for (;;) {
		if (gettimeofday(&tnow, NULL) != 0) {
			LERR("gettimeofday() failed with errno=%d (%s)\n", errno, strerror(errno));
			return -1;
		}
		td=(tnow.tv_usec-tstart.tv_usec+(tnow.tv_sec-tstart.tv_sec)*1000000) / 1000;
		if (td > timeout) {
			LWARNING("exceeded timeout while waiting for response.\n");
			return -1;
		}
		
		ret=poll(pfds, 1, timeout-td);
		if (pfds->revents & POLLIN) {
			if (recv(pfds->fd, buf, NETBUFSIZE, MSG_DONTWAIT) > 0) { /* do not block - just in case select/poll was wrong */
				buf[NETBUFSIZE] = '\0';
				if (strcmp(buf, number) == 0) {
					carrierid=ntohs(*((short int *)&(buf[reqlen]))); /* convert to host byte order */
					goto found;
				}
			}
		}
		pfds->revents = 0;
	}

	found:
	if (gettimeofday(&tnow, NULL) == 0) {
		LINFO("got an answer in %f ms\n", ((double)(tnow.tv_usec-tstart.tv_usec+(tnow.tv_sec-tstart.tv_sec)*1000000))/1000);
	}
	return carrierid;
}




struct server_query_data_t {
	int timeout;
	struct sockaddr_in dstaddr;
	socklen_t dstaddrlen;
	struct pollfd pfds;
};




void query_mmap(char *number, char *comment, void *data) {
	int nmatch;
	carrier_t carrierid;
	struct dtm_node_t *mroot = (struct dtm_node_t *)data;

	nmatch=dtm_longest_match(mroot, number, strlen(number), &carrierid);

	if (nmatch<=0) {
		LINFO("%s:%s:%ld:%s\n", number, comment, (long int)carrierid, "not allocated, probably old");
	}
	else {
		LINFO("%s:%s:%ld:%s\n", number, comment, (long int)carrierid, carrierid2name(carrierid));
		/* LINFO("%s: found: carrier_id=%ld, carrier_name='%s', nmatch=%ld, comment='%s'\n", number, (long int)carrierid, carrierid2name(carrierid), (long int)nmatch, comment);
		*/
	}
}




void query_server(char *number, char *comment, void *data) {
	carrier_t carrierid;
	struct server_query_data_t *sdata = (struct server_query_data_t *)data;

	carrierid = query_udp(number, sdata->timeout, &(sdata->pfds), &(sdata->dstaddr), sdata->dstaddrlen);

	if (carrierid<=0) {
		LINFO("%s: not_found: comment='%s'\n", number, comment);
	}
	else {
		LINFO("%s:%ld:%s\n", number, (long int)carrierid, carrierid2name(carrierid));
		/* LINFO("%s: found: carrier_id=%ld, carrier_name='%s', comment='%s'\n", number, (long int)carrierid, carrierid2name(carrierid), comment);
		*/
	}
}




int main(int argc, char *argv[]) {
	int n;
	struct dt_node_t root;
	memset(&root, 0, sizeof(root));
	struct dtm_node_t *mroot;

	int opt;
	char *csv_file = NULL;
	char *mmap_file = NULL;
	char *query_file = NULL;
	char *tree_file = NULL;
	int optimize = 0;
	int keep_carriers_num = 0;
	carrier_t keep_carriers[MAX_PDB_CARRIERID+1];
	char *host_str = NULL;
	char *port_str = NULL;
	unsigned short int port = 0;
	char *tmp;
	int log_level = LOG_INFO;

	struct hostent *hp;
	int sockfd;

	struct server_query_data_t sdata;

	char *id_str;
	long int ret;

	sdata.timeout=500;

	init_carrier_names();

	init_log("pdbt", 0);

	while ((opt = getopt(argc, argv, "s:m:f:u:t:r:q:k:ol:h")) != -1) {
		switch (opt) {
		case 's':
			csv_file = optarg;
			break;
		case 'm':
			mmap_file = optarg;
			break;
		case 'f':
			query_file = optarg;
			break;
		case 'u':
			tree_file = optarg;
			break;
		case 'k':
			while ((id_str=strsep(&optarg, ","))) {
				ret=strtol(id_str, NULL, 10);
				if (!IS_VALID_PDB_CARRIERID(ret)) {
					LERR("invalid carrier id '%s' specified.\n", id_str);
					return -1;
				}
				if (keep_carriers_num>MAX_PDB_CARRIERID) {
					LERR("too many carrier ids specified.\n");
					return -1;
				}
				keep_carriers[keep_carriers_num]=ret;
				keep_carriers_num++;
			}
			break;
		case 't':
			if (load_carrier_names(optarg)<0) {
				LERR("cannot load carrier names from '%s'.\n", optarg);
				return -1;
			}
			break;
		case 'r':
			host_str=optarg;

			tmp = strchr(host_str, ':');
			if (tmp == NULL) {
				LERR("syntax error in remote host:port specification '%s'.\n", host_str);
				return -1;
			}
			*tmp = '\0';
			port_str = tmp + 1;

			ret=strtol(port_str, NULL, 10);
			if ((ret<0) || (ret==LONG_MAX)) {
				LERR("invalid timeout '%s'\n", optarg);
				return -1;
			}
			port = ret;

			break;
		case 'q':
			ret=strtol(optarg, NULL, 10);
			if ((ret<0) || (ret>65535)) {
				LERR("invalid port '%s'\n", port_str);
				return -1;
			}
			sdata.timeout = ret;

			break;
		case 'o':
			optimize=1;
			break;
		case 'l':
			ret=strtol(optarg, NULL, 10);
			if ((ret<LOG_EMERG-1) || (ret>LOG_DEBUG)) {
				LERR("invalid log level '%s' specified.\n", optarg);
				return -1;
			}
			log_level=ret;
			break;
		case 'h':
			print_usage(argv[0]);
			return 0;
			break;
		default:
			LERR("invalid option '%c'.\n", opt);
			print_usage(argv[0]);
			return 1;
		}
	}

	set_log_level(log_level);

	if (optind>=argc) {
		LERR("no command specified.\n");
		return 1;
	}

	if (strcmp(argv[optind], "build")==0) {
		if (csv_file==NULL) {
			LERR("no csv file specified.\n");
			return 1;
		}

		if (mmap_file==NULL) {
			LERR("no mmap file specified.\n");
			return 1;
		}

		LINFO("loading '%s'...\n", csv_file);
		n = import_csv(&root, csv_file);
		if (n < 0) {
			LERR("cannot import '%s'\n", csv_file);
			return -1;
		}
		LINFO("done.\n");
		LINFO("%ld lines imported\n", (long int)n);

		LINFO("Node size is %ld bytes (%ld for dtm)\n", (long int)sizeof(struct dt_node_t), (long int)sizeof(struct dtm_node_t));
		print_stats(&root);

		if (keep_carriers_num) {
			LINFO("merging carriers...\n");
			n=merge_carrier(&root, keep_carriers_num, keep_carriers);
			LINFO("done (modified %ld nodes).\n", (long int)n);
		}

		if (optimize) {
			LINFO("optimizing...\n");
			dt_optimize(&root);
			LINFO("done.\n");
			print_stats(&root);
		}

		if (tree_file != NULL) {
			LINFO("writing human-readable tree...\n");
			if (dt_write_tree(&root, tree_file) < 0) {
				LERR("cannot write tree\n");
				return -1;
		    }
			LINFO("done.\n");
		}

		LINFO("saving to '%s'...\n", mmap_file);
		n = save_mmap(&root, mmap_file);
		if (n < 0) {
			LERR("cannot save '%s'\n", mmap_file);
			return -1;
		}
		LINFO("done.\n");
		LINFO("%ld nodes saved\n", (long int)n);
	}
	else if (strcmp(argv[optind], "query")==0) {
		if ((mmap_file!=NULL) && (host_str!=NULL)) {
			LERR("you cannot query a pdb_server and a mmap file at the same time.\n");
			return 1;
		}

		if ((mmap_file==NULL) && (host_str==NULL)) {
			LERR("neither a mmap file nor a remote host specified.\n");
			return 1;
		}

		if (mmap_file==NULL) {
			sockfd = socket(AF_INET, SOCK_DGRAM, 0);
			if (sockfd<0) {
				LERR("socket() failed with errno=%d (%s).\n", errno, strerror(errno));
				return -1;
			}

			memset(&sdata.dstaddr, 0, sizeof(sdata.dstaddr));
			sdata.dstaddr.sin_family = AF_INET;
			sdata.dstaddr.sin_port = htons(port);
			hp = gethostbyname(host_str);
			if (hp == NULL) {
				LERR("gethostbyname(%s) failed with h_errno=%d.\n", host_str, h_errno);
				close(sockfd);
				return -1;
			}
			memcpy(&sdata.dstaddr.sin_addr.s_addr, hp->h_addr, hp->h_length);
			sdata.dstaddrlen=sizeof(sdata.dstaddr);

			sdata.pfds.fd=sockfd;
			sdata.pfds.events=POLLIN;

			if (query_file==NULL) {
				LINFO("\nprocessing command line parameters...\n");
				for (n=optind+1; n<argc; n++) {
					query_server(argv[n], "", &sdata);
				}
			}
			else {
				file_query(query_file, query_server, &sdata);
			}
		}
		else {
			mroot=dtm_load(mmap_file);
			if (mroot == NULL) {
				LERR("cannot load '%s'.\n", mmap_file);
				return -1;
			}
			
			if (query_file==NULL) {
				LINFO("\nprocessing command line parameters...\n");
				for (n=optind+1; n<argc; n++) {
					query_mmap(argv[n], "", mroot);
				}
			}
			else {
				file_query(query_file, query_mmap, mroot);
			}
		}
	}
	else {
		LERR("invalid command '%s'.\n", argv[optind]);
		return 1;
	}

	return 0;
}
