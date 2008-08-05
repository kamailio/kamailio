#include "orasel.h"
#include <ctype.h>
#include <unistd.h>

outmode_t outmode;

//-----------------------------------------------------------------------------
static void prepare_uri(con_t* con, const char *uri)
{
	const char *p = strchr(uri, '/');

	if(!p || p == uri) goto bad;
	con->username = str_alloc(uri, p - uri);
	uri = p+1;
	p = strchr(uri, '@');
	if(!p || p == uri) goto bad;
	con->password = str_alloc(uri, p - uri);
	if(strchr(con->password->s, '/')) goto bad;
	if(!*++p) goto bad;
	con->uri = str_alloc(p, strlen(p));
	return;

bad:
	errxit("invalid db (must be as name/password@dbname)");
}

//-----------------------------------------------------------------------------
static const Str* prepare_req(const char* req)
{
	Str* ps;
	char *p;

	while(*req && isspace((unsigned char)*req)) ++req;
	if(strncasecmp(req, "select", sizeof("select")-1)) goto bad;
	p = (char*)req + sizeof("select")-1;
	if(!*p || !isspace((unsigned char)*p)) goto bad;
	ps = str_alloc(req, strlen(req));
	p = (char*) ps->s + sizeof("select")-1;
	do if(isspace((unsigned char)*p)) *p = ' '; while(*++p);
	do --p; while(isspace((unsigned char)*p));
	if(*p != ';') goto bad;
	do {
		do --p; while(isspace((unsigned char)*p));
	}while(*p == ';');
	*++p = '\0';
	ps->len = p - ps->s;
	if(ps->len <= sizeof("select")-1) {
bad:
		errxit("support only 'select ...;' request");
	}
	return ps;
}

//-----------------------------------------------------------------------------
static void get_opt(int argc, char* argv[])
{
	int opt = 0;

	if(argc <= 1 || (argc == 2 && !strcmp(argv[1], "--help"))) {
help:
		fprintf(stderr, "Kamailio for oracle 'select' request utility\n");
		opt = -2;  /* flag for help print */
	} else {
		while((opt = getopt(argc-1, argv+1, "BLNe:")) != -1) {
			switch(opt) {
			case 'B':
				outmode.raw = 1;;
				break;
			case 'L':
				outmode.hdr = 1;
				break;
			case 'N':
				outmode.emp = 1;
				break;
			case 'e':
				if(optind == argc-1) return;
				// pass thru
			default:
				goto help;
			}
		}
	}
	fprintf(stderr, "use: %s user/password@db [-BLN] -e \"select ...;\"\n",
		argv[0]);
	if(opt == -2) {
		fprintf(stderr, "     where   -B - print using tab separator\n");
		fprintf(stderr, "             -L - skip column headers\n");
		fprintf(stderr, "             -N - skip notify of empty result\n");
	}
	exit(1);
}

//-----------------------------------------------------------------------------
int main(int argc, char* argv[])
{
	con_t con;
	res_t res;
	const Str* req;


	get_opt(argc, argv);

	memset(&con, 0, sizeof(con));
	memset(&res, 0, sizeof(res));

	prepare_uri(&con, argv[1]);
	req = prepare_req(optarg);
	open_sess(&con);
	send_req(&con, req);
	get_res(&con, &res);
	OCITerminate(OCI_DEFAULT);
	out_res(&res);
	return 0;
}

//-----------------------------------------------------------------------------
