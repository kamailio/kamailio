/*
 * $Id$
 *
 */

#ifndef route_struct_h
#define route_struct_h

#define EXPR_DROP -127  /* used only by the expression and if evaluator */
/*
 * Other important values (no macros for them yet):
 * expr true = 1
 * expr false = 0 (used only inside the expression and if evaluator)
 * 
 * action continue  or if used in condition true = 1
 * action drop/quit/stop script processing = 0
 * action error or if used in condition false = -1 (<0 and !=EXPR_DROP)
 * 
 */


enum { EXP_T=1, ELEM_T };
enum { AND_OP=1, OR_OP, NOT_OP };
enum { EQUAL_OP=10, MATCH_OP, NO_OP };
enum { METHOD_O=1, URI_O, SRCIP_O, DSTIP_O, DEFAULT_O, ACTION_O, NUMBER_O};

enum { FORWARD_T=1, SEND_T, DROP_T, LOG_T, ERROR_T, ROUTE_T, EXEC_T,
		SET_HOST_T, SET_HOSTPORT_T, SET_USER_T, SET_USERPASS_T, 
		SET_PORT_T, SET_URI_T, IF_T, MODULE_T,
		SETFLAG_T, RESETFLAG_T, ISFLAGSET_T ,
		LEN_GT_T, PREFIX_T, STRIP_T };
enum { NOSUBTYPE=0, STRING_ST, NET_ST, NUMBER_ST, IP_ST, RE_ST, PROXY_ST,
		EXPR_ST, ACTIONS_ST, CMDF_ST, MODFIXUP_ST, URIHOST_ST, URIPORT_ST };

	
struct expr{
	int type; /* exp, exp_elem */
	int op; /* and, or, not | ==,  =~ */
	int  subtype;
	union {
		struct expr* expr;
		int operand;
	}l;
	union {
		struct expr* expr;
		void* param;
		int   intval;
	}r;
};


struct action{
	int type;  /* forward, drop, log, send ...*/
	int p1_type;
	int p2_type;
	int p3_type;
	union {
		int number;
		char* string;
		void* data;
	}p1, p2, p3;
	struct action* next;
};


struct net{
	unsigned long ip;
	unsigned long mask;
};

struct expr* mk_exp(int op, struct expr* left, struct expr* right);
struct expr* mk_elem(int op, int subtype, int operand, void* param);
struct action* mk_action(int type, int p1_type, int p2_type,
							void* p1, void* p2);
struct action* mk_action3(int type, int p1_type, int p2_type, int p3_type, 
							void* p1, void* p2, void* p3);
struct action* append_action(struct action* a, struct action* b);


struct net* mk_net(unsigned long ip, unsigned long mask);

void print_action(struct action* a);
void print_expr(struct expr* exp);





#endif

