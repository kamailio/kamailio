/*
 * $Id$
 *
 * Copyright (c) 2001  Jordan Ritter <jpr5@darkridge.com>
 *
 * Please refer to the COPYRIGHT file for more information. 
 * 
 */


#if defined(BSD) || defined(SOLARIS) || defined(MACOSX)
#include <unistd.h>
#include <ctype.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <netinet/in_systm.h>
#include <net/if.h>
#include <sys/tty.h>
#endif

#if defined(OSF1)
#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <netinet/in_systm.h>
#include <net/route.h>
#include <sys/mbuf.h>
#endif

#if defined(LINUX)
#include <getopt.h>
#include <arpa/inet.h>
#include <ctype.h>
#include <time.h>
#endif

#if defined(AIX)
#include <sys/machine.h>
#include <sys/types.h>
#include <netinet/in.h>
#include <time.h>
#endif

#include <netinet/ip.h>
#include <netinet/tcp.h>
#include <netinet/udp.h>
#include <netinet/ip_icmp.h>

#include <pcap.h>
#include <net/bpf.h>

#include <stdlib.h>
#include <string.h>
#include <signal.h>
#include <sys/ioctl.h>

#include <locale.h>

#ifdef USE_PCRE
#include "pcre-3.4/pcre.h"
#else
#include "regex-0.12/regex.h"
#endif

#include "ngrep.h"

#define dump(_a,_b) ( show_eol ? \
	dump_line_by_line((_a),(_b)) : _dump((_a),(_b)) )


static char rcsver[] = "$Revision$";

int snaplen = 65535, promisc = 1, to = 1000;
int show_empty = 0, show_hex = 0, quiet = 0;
int match_after = 0, keep_matching = 0;
int invert_match = 0, bin_match = 0;
int matches = 0, max_matches = 0;
int live_read = 1, want_delay = 0;

int show_eol=0;

char pc_err[PCAP_ERRBUF_SIZE];
#ifdef USE_PCRE
int err_offset;
char *re_err = NULL;
#else
const char *re_err = NULL;
#endif

int re_match_word = 0, re_ignore_case = 0;

#ifdef USE_PCRE
pcre *pattern = NULL;
pcre_extra *pattern_extra = NULL;
#else 
struct re_pattern_buffer pattern;
#endif

char *match_data = NULL, *bin_data = NULL, *filter = NULL;
int (*match_func)() = &blank_match_func;
int match_len = 0;

struct bpf_program pcapfilter;
struct in_addr net, mask;
pcap_t *pd = NULL;
char *dev = NULL;
int link_offset;

char *read_file = NULL, *dump_file = NULL;
pcap_dumper_t *pd_dump = NULL;

struct timeval prev_ts = {0, 0}, prev_delay_ts = {0,0};
void (*print_time)() = NULL, (*dump_delay)() = dump_delay_proc_init;

unsigned ws_row, ws_col;


int main(int argc, char **argv) {
  int c;

  signal(SIGINT,  clean_exit);
  signal(SIGQUIT, clean_exit);
  signal(SIGABRT, clean_exit);
  signal(SIGPIPE, clean_exit);
  signal(SIGWINCH, update_windowsize);

  setlocale(LC_ALL, "");

  while ((c = getopt(argc, argv, "LhXViwqpevxlDtTs:n:d:A:I:O:")) != EOF) {
    switch (c) {
	case 'L':
		show_eol=1;
		break;
    case 'I':  
      read_file = optarg;
      break;
    case 'O':
      dump_file = optarg;
      break;
    case 'A': 
      match_after = atoi(optarg) + 1;
      break;
    case 'd': 
      dev = optarg;
      break;
    case 'n':
      max_matches = atoi(optarg);
      break;
    case 's':
      snaplen = atoi(optarg);
      break;
    case 'T':
      print_time = &print_time_diff_init;
      break;
    case 't':
      print_time = &print_time_absolute;
      break;
    case 'D':
      want_delay = 1;
      break;
    case 'l':
      setvbuf(stdout, NULL, _IOLBF, 0);
      break;
    case 'x':
      show_hex++;
      break;
    case 'v':
      invert_match++;
      break;
    case 'e':
      show_empty++;
      break;
    case 'p':
      promisc = 0;
      break;
    case 'q':
      quiet++;
      break;
    case 'w':
      re_match_word++;
      break;
    case 'i':
      re_ignore_case++;
      break;
    case 'V':
      version();
    case 'X':
      bin_match++;
      break;
    case 'h':
      usage(0);
    default:
      usage(-1);
    }
  }


  if (argv[optind]) 
    match_data = argv[optind++];

  if (read_file) {
    if (!(pd = pcap_open_offline(read_file, pc_err))) {
      perror(pc_err);
      clean_exit(-1);
    }

    live_read = 0;
    printf("input: %s\n", read_file);

  } else {
    if (!dev)
      if (!(dev = pcap_lookupdev(pc_err))) {
	perror(pc_err);
	clean_exit(-1);
      }
    
    if ((pd = pcap_open_live(dev, snaplen, promisc, to, pc_err)) == NULL) {
      perror(pc_err);
      clean_exit(-1);
    }

    if (pcap_lookupnet(dev, &net.s_addr, &mask.s_addr, pc_err) == -1) {
      perror(pc_err);
      memset(&net, 0, sizeof(net));
      memset(&mask, 0, sizeof(mask));
    } 

    if (!quiet) {
      printf("interface: %s", dev);
      if (net.s_addr && mask.s_addr) {
	printf(" (%s/", inet_ntoa(net));
	printf("%s)", inet_ntoa(mask)); 
      }
      printf("\n");
    }
  }


  if (argv[optind]) {
    filter = get_filter(&argv[optind]); 

    if (pcap_compile(pd, &pcapfilter, filter, 0, mask.s_addr)) {
      free(filter); 
      filter = get_filter(&argv[optind-1]); 

#ifdef NEED_RESTART
      PCAP_RESTART();
#endif
      if (pcap_compile(pd, &pcapfilter, filter, 0, mask.s_addr)) {
	pcap_perror(pd, "pcap compile");
	clean_exit(-1);
      } else match_data = NULL;
    }

    if (!quiet) printf("filter: %s\n", filter); 
    
    if (pcap_setfilter(pd, &pcapfilter)) {
      pcap_perror(pd, "pcap set");
      clean_exit(-1);
    }
  }

  if (match_data) {
    if (bin_match) {
      int i = 0, n;
      char *s, *d;
      int len;

      if (re_match_word || re_ignore_case) {
	fprintf(stderr, "fatal: regex switches are incompatible with binary matching\n");
	clean_exit(-1);
      }

      len = strlen(match_data);
      if (len % 2 != 0 || !strishex(match_data)) {
	fprintf(stderr, "fatal: invalid hex string specified\n");
	clean_exit(-1);
      }

      bin_data = malloc(len / 2);
      memset(bin_data, 0, len / 2);
      d = bin_data;

      if ((s = strchr(match_data, 'x'))) 
	len -= ++s - match_data - 1;
      else s = match_data;

      while (i <= len) {
	sscanf(s+i, "%2x", &n);
	*d++ = n;
	i += 2;
      }

      match_len = len / 2;
      match_func = &bin_match_func;

    } else {

#ifdef USE_PCRE
      int pcre_options = PCRE_UNGREEDY;

      if (re_ignore_case) 
	pcre_options |= PCRE_CASELESS;
      
      re_err = malloc(512);
#else
      re_syntax_options = RE_SYNTAX_EGREP;
      
      if (re_ignore_case) {
	char *s;
	int i;
	
	pattern.translate = (char*)malloc(256);
	s = pattern.translate;
	for (i = 0; i < 256; i++) 
	  s[i] = i;
	for (i = 'A'; i <= 'Z'; i++) 
	  s[i] = i + 32;

	s = match_data;
	while (*s) 
	  *s++ = tolower(*s);
      } else pattern.translate = NULL;
#endif

      if (re_match_word) {
	char *word_regex = malloc(strlen(match_data) * 3 + strlen(WORD_REGEX));
	sprintf(word_regex, WORD_REGEX, match_data, match_data, match_data);
	match_data = word_regex;
      }

#ifdef USE_PCRE
      pattern = pcre_compile(match_data, pcre_options, (const char **)&re_err, &err_offset, 0);
      if (!pattern) {
	fprintf(stderr, "compile failed: %s\n", re_err);
	clean_exit(-1);
      }

      pattern_extra = pcre_study(pattern, 0, (const char **)&re_err);
      
      free(re_err);
      re_err = NULL;
#else
      re_err = re_compile_pattern(match_data, strlen(match_data), &pattern);
      if (re_err) {
	fprintf(stderr, "regex compile: %s\n", re_err);
	clean_exit(-1);
      }

      pattern.fastmap = (char*)malloc(256);
      if (re_compile_fastmap(&pattern)) {
	perror("fastmap compile failed");
	clean_exit(-1);
      }
#endif

      match_func = &re_match_func;
    }

    if (!quiet && match_data && strlen(match_data)) 
      printf("%smatch: %s%s\n", invert_match?"don't ":"", 
	     (bin_data && !strchr(match_data, 'x'))?"0x":"", match_data);
  }


  if (filter) free(filter);
  if (re_match_word) free(match_data);


  switch(pcap_datalink(pd)) {
  case DLT_EN10MB:
    link_offset = ETHHDR_SIZE;
    break;

  case DLT_IEEE802:
    link_offset = TOKENRING_SIZE;
    break;
    
  case DLT_FDDI:
    link_offset = FDDIHDR_SIZE;
    break;

  case DLT_SLIP: 
    link_offset = SLIPHDR_SIZE;
    break;
    
  case DLT_PPP:
    link_offset = PPPHDR_SIZE;
    break;

  case DLT_RAW: 
    link_offset = RAWHDR_SIZE;
    break;

  case DLT_LOOP:
  case DLT_NULL:
    link_offset = LOOPHDR_SIZE;
    break;

  case DLT_LINUX_SLL:
    link_offset = ISDNHDR_SIZE;
    break;

  default:
    fprintf(stderr, "fatal: unsupported interface type %d\n", pcap_datalink(pd));
    clean_exit(-1);
  }
  
  if (dump_file) {
    if (!(pd_dump = pcap_dump_open(pd, dump_file))) {
      fprintf(stderr, "fatal: %s\n", pcap_geterr(pd));
      clean_exit(-1);
    } else printf("output: %s\n", dump_file);
  }

  update_windowsize(0);

  while (pcap_loop(pd, 0, (pcap_handler)process, 0));

  clean_exit(0);
}	


void process(u_char *data1, struct pcap_pkthdr* h, u_char *p) {
  struct ip* ip_packet = (struct ip *)(p + link_offset);

#if defined(AIX)
#undef ip_hl
  unsigned ip_hl = ip_packet->ip_ff.ip_fhl*4;
#else
  unsigned ip_hl = ip_packet->ip_hl*4;
#endif

  unsigned ip_off = ntohs(ip_packet->ip_off);
  unsigned fragmented = ip_off & (IP_MF | IP_OFFMASK);
  unsigned frag_offset = fragmented?(ip_off & IP_OFFMASK) * 8:0;

  char *data;
  int len;

  switch (ip_packet->ip_p) {
  case IPPROTO_TCP: {
    struct tcphdr* tcp = (struct tcphdr *)(((char *)ip_packet) + ip_hl);
    unsigned tcphdr_offset = fragmented?0:(tcp->th_off * 4);

    if (!quiet) {
      printf("#");
      fflush(stdout);
    }
    
    data = ((char*)tcp) + tcphdr_offset;

    if ((len = ntohs(ip_packet->ip_len)) < h->caplen)
      len -= ip_hl + tcphdr_offset;
    else len = h->caplen - link_offset - ip_hl - tcphdr_offset;

    if (((len || show_empty) && (((int)(*match_func)(data, len)) != invert_match))
	|| keep_matching) { 

      if (!live_read && want_delay)
	dump_delay(h);

      printf("\nT ");

      if (print_time) 
	print_time(h);

      if (tcphdr_offset || !frag_offset) {
	printf("%s:%d -", inet_ntoa(ip_packet->ip_src), ntohs(tcp->th_sport));
	printf("> %s:%d", inet_ntoa(ip_packet->ip_dst), ntohs(tcp->th_dport));
	printf(" [%s%s%s%s%s%s]",
	       (tcp->th_flags & TH_ACK)?"A":"",
	       (tcp->th_flags & TH_SYN)?"S":"",
	       (tcp->th_flags & TH_RST)?"R":"",
	       (tcp->th_flags & TH_FIN)?"F":"",
	       (tcp->th_flags & TH_URG)?"U":"",
	       (tcp->th_flags & TH_PUSH)?"P":"");
      } else {
	printf("%s -", inet_ntoa(ip_packet->ip_src));
	printf("> %s", inet_ntoa(ip_packet->ip_dst));
      }

      if (fragmented) 
	printf(" %s%d@%d:%d\n", frag_offset?"+":"", ntohs(ip_packet->ip_id),
                                frag_offset, len); 
      else printf("\n");
      
      if (pd_dump) {
	pcap_dump((u_char*)pd_dump, h, p);
	if (!quiet) dump(data, len);
      } else dump(data, len);
    }
  }
  break;

  case IPPROTO_UDP: {
    struct udphdr* udp = (struct udphdr *)(((char *)ip_packet) + ip_hl);
    unsigned udphdr_offset = (fragmented)?0:sizeof(struct udphdr); 

    if (!quiet) {
      printf("#"); 
      fflush(stdout);
    }

    data = ((char*)udp) + udphdr_offset;

    if ((len = ntohs(ip_packet->ip_len)) < h->caplen)
      len -= ip_hl + udphdr_offset;
    else len = h->caplen - link_offset - ip_hl - udphdr_offset;

    if (((len || show_empty) && (((int)(*match_func)(data, len)) != invert_match))
	|| keep_matching) { 

      if (!live_read && want_delay)
	dump_delay(h);

      printf("\nU ");

      if (print_time) 
	print_time(h);

      if (udphdr_offset || !frag_offset) {
#ifdef HAVE_DUMB_UDPHDR
	printf("%s:%d -", inet_ntoa(ip_packet->ip_src), ntohs(udp->source));
	printf("> %s:%d", inet_ntoa(ip_packet->ip_dst), ntohs(udp->dest));
#else
	printf("%s:%d -", inet_ntoa(ip_packet->ip_src), ntohs(udp->uh_sport));
	printf("> %s:%d", inet_ntoa(ip_packet->ip_dst), ntohs(udp->uh_dport));
#endif
      } else {
	printf("%s -", inet_ntoa(ip_packet->ip_src));
	printf("> %s", inet_ntoa(ip_packet->ip_dst));
      }

      if (fragmented) 
	printf(" %s%d@%d:%d\n", frag_offset?"+":"", ntohs(ip_packet->ip_id),
	       frag_offset, len); 
      else printf("\n");
      
      if (pd_dump) {
	pcap_dump((u_char*)pd_dump, h, p);
	if (!quiet) dump(data, len);
      } else dump(data, len);
    }
  }
  break;

  case IPPROTO_ICMP: {
    struct icmp* ic = (struct icmp *)(((char *)ip_packet) + ip_hl);
    unsigned icmphdr_offset = fragmented?0:4;

    if (!quiet) {
      printf("#"); 
      fflush(stdout);
    }

    data = ((char*)ic) + icmphdr_offset;

    if ((len = ntohs(ip_packet->ip_len)) < h->caplen)
      len -= ip_hl + icmphdr_offset;
    else len = h->caplen - link_offset - ip_hl - icmphdr_offset;

    if (((len || show_empty) && (((int)(*match_func)(data, len)) != invert_match))
	|| keep_matching) { 

      if (!live_read && want_delay)
	dump_delay(h);

      printf("\nI ");

      if (print_time) 
	print_time(h);

      printf("%s -", inet_ntoa(ip_packet->ip_src));
      printf("> %s", inet_ntoa(ip_packet->ip_dst));

      if (icmphdr_offset || !frag_offset) 
	printf(" %d:%d", ic->icmp_type, ic->icmp_code);

      if (fragmented) 
	printf(" %s%d@%d:%d\n", frag_offset?"+":"", ntohs(ip_packet->ip_id),
                                frag_offset, len); 
      else printf("\n");

      if (pd_dump) {
	pcap_dump((u_char*)pd_dump, h, p);
	if (!quiet) dump(data, len);
      } else dump(data, len);
    }
  }
  break;
  
  }

  if (match_after && keep_matching)
    keep_matching--;
}


int re_match_func(char *data, int len) {
#ifdef USE_PCRE
  switch(pcre_exec(pattern, 0, data, len, 0, 0, 0, 0)) {
   case PCRE_ERROR_NULL:
   case PCRE_ERROR_BADOPTION:
   case PCRE_ERROR_BADMAGIC: 
   case PCRE_ERROR_UNKNOWN_NODE: 
   case PCRE_ERROR_NOMEMORY:
     perror("she's dead, jim\n");
     clean_exit(-2);

   case PCRE_ERROR_NOMATCH: 
     return 0;
  }
#else
  switch (re_search(&pattern, data, len, 0, len, 0)) {
   case -2: 
     perror("she's dead, jim\n");
     clean_exit(-2);

   case -1:
     return 0;
  }
#endif
  
  if (max_matches && ++matches > max_matches)
    clean_exit(0);

  if (match_after && keep_matching != match_after)
    keep_matching = match_after;

  return 1;
}


int bin_match_func(char *data, int len) {
  int stop = len - match_len;
  int i = 0;

  if (stop < 0)
    return 0;

  while (i <= stop) 
    if (!memcmp(data+(i++), bin_data, match_len)) {
      if (max_matches && ++matches > max_matches)
	clean_exit(0);

      if (match_after && keep_matching != match_after)
	keep_matching = match_after;

      return 1;
    }

  return 0;
}


int blank_match_func(char *data, int len) {
  if (max_matches && ++matches > max_matches)
    clean_exit(0);

  return 1;
}

void dump_line_by_line( char *data, int len )
{
	unsigned width;
	char *c;
	
	c=data;

	while( c< data + len) {
		if (*c=='\n') {
			puts("");
		} else {
	  		putchar(isprint(*c)?*c:'.');
		}
		c++;

	}
	puts("");

}


void _dump(char *data, int len) {  
  if (len > 0) {
    unsigned width = show_hex?16:(ws_col-5);
    char *str = data;
    int j, i = 0;

    while (i < len) {
      printf("  ");

      if (show_hex) 
	for (j = 0; j < width; j++) {
	  if (i+j < len) 
	    printf("%02x ", (unsigned char)str[j]);
	  else printf("   ");

	  if ((j+1) % (width/2) == 0)
	    printf("   ");
	}

      for (j = 0; j < width; j++) 
	if (i+j < len) 
	  printf("%c", isprint(str[j])?str[j]:'.');
	else printf(" ");
      
      str += width;
      i += j;

      printf("\n");
    }
  }
}


char *get_filter(char **argv) {
  char **arg = argv, *theirs, *mine;
  char *from, *to;
  int len = 0;

  if (!*arg)
    return NULL;

  while (*arg) 
    len += strlen(*arg++) + 1;

  if (!(theirs = (char*)malloc(len + 1)) || 
      !(mine = (char*)malloc(len + sizeof(IP_ONLY))))
    return NULL;

  memset(theirs, 0, len + 1);
  memset(mine, 0, len + sizeof(IP_ONLY));

  arg = argv;
  to = theirs;

  while ((from = *arg++)) {
    while ((*to++ = *from++));
    *(to-1) = ' ';
  }

  sprintf(mine, IP_ONLY, theirs);

  free(theirs);
  return mine;
}


int strishex(char *str) {
  char *s;
  if ((s = strchr(str, 'x'))) 
    s++;
  else s = str;

  while (*s) 
    if (!isxdigit(*s++))
      return 0;

  return 1;
}


void print_time_absolute(struct pcap_pkthdr *h) {
#ifdef MACOSX
  struct tm *t = localtime((const time_t *)&h->ts.tv_sec);
#else
  struct tm *t = localtime(&h->ts.tv_sec);
#endif

  printf("%02d/%02d/%02d %02d:%02d:%02d.%06d ",
	 t->tm_year+1900, t->tm_mon+1, t->tm_mday, t->tm_hour,
	 t->tm_min, t->tm_sec, h->ts.tv_usec);
}


void print_time_diff_init(struct pcap_pkthdr *h) {
  print_time = &print_time_diff;

  prev_ts.tv_sec = h->ts.tv_sec;
  prev_ts.tv_usec = h->ts.tv_usec;
  
  print_time(h);
}

void print_time_diff(struct pcap_pkthdr *h) { 
  unsigned secs, usecs;

  secs = h->ts.tv_sec - prev_ts.tv_sec;
  if (h->ts.tv_usec >= prev_ts.tv_usec)
    usecs = h->ts.tv_usec - prev_ts.tv_usec;
  else {
    secs--; 
    usecs = 1000000 - (prev_ts.tv_usec - h->ts.tv_usec);
  }

  printf("+%d.%06d ", secs, usecs);

  prev_ts.tv_sec = h->ts.tv_sec;
  prev_ts.tv_usec = h->ts.tv_usec;
}

void dump_delay_proc_init(struct pcap_pkthdr *h) {
  dump_delay = &dump_delay_proc;

  prev_delay_ts.tv_sec = h->ts.tv_sec;
  prev_delay_ts.tv_usec = h->ts.tv_usec;

  dump_delay(h);
}

void dump_delay_proc(struct pcap_pkthdr *h) {
  unsigned secs, usecs;

  secs = h->ts.tv_sec - prev_delay_ts.tv_sec;
  if (h->ts.tv_usec >= prev_delay_ts.tv_usec)
    usecs = h->ts.tv_usec - prev_delay_ts.tv_usec;
  else {
    secs--; 
    usecs = 1000000 - (prev_delay_ts.tv_usec - h->ts.tv_usec);
  }

  sleep(secs);
  usleep(usecs);
  
  prev_delay_ts.tv_sec = h->ts.tv_sec;
  prev_delay_ts.tv_usec = h->ts.tv_usec;
}


void update_windowsize(int e) {
  const struct winsize ws;
  
  if (!ioctl(0, TIOCGWINSZ, &ws)) {
    ws_row = ws.ws_row;
    ws_col = ws.ws_col;
  } else {
    ws_row = 24;
    ws_col = 80;
  }
}


void usage(int e) {
  printf("usage: ngrep <-hLXViwqpevxlDtT> <-IO pcap_dump> <-n num> <-d dev> <-A num>\n"
	 "                               <-s snaplen> <match expression> <bpf filter>\n");
  exit(e);
}


void version(void) {
  printf("ngrep: V%s, %s\n", VERSION, rcsver);
  exit(0);
}


void clean_exit(int sig) {
  struct pcap_stat s;
  if (!quiet && sig >= 0) printf("exit\n");

#ifdef USE_PCRE
  if (re_err) free(re_err);
  if (pattern) pcre_free(pattern);
  if (pattern_extra) pcre_free(pattern_extra);
#else
  if (pattern.translate) free(pattern.translate);
  if (pattern.fastmap) free(pattern.fastmap);
#endif

  if (bin_data) free(bin_data);
  
  if (!quiet && sig >= 0 && !read_file &&
      pd && !pcap_stats(pd, &s)) 
    printf("%d received, %d dropped\n", s.ps_recv, s.ps_drop);

  if (pd) pcap_close(pd);
  if (pd_dump) pcap_dump_close(pd_dump);

  exit(sig);
}


