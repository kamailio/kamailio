/*
 * $Id$
 *
 */

#ifndef route_struct_h
#define route_struct_h

enum { EXP_T=1, ELEM_T };
enum { AND_OP=1, OR_OP, NOT_OP };
enum { EQUAL_OP=10, MATCH_OP };
enum { METHOD_O=1, URI_O, SRCIP_O, DSTIP_O };

enum { FORWARD_T=1, SEND_T, DROP_T, LOG_T, ERROR_T};
enum { NOSUBTYPE=0, STRING_ST, NET_ST, NUMBER_ST, IP_ST };

	
struct expr{
	int type; /* exp, exp_elem */
	int op; /* and, or, not | ==,  ~= */
	int  subtype;
	union {
		struct expr* expr;
		int operand;
	}l;
	union {
		struct expr* expr;
		void* param;
	}r;
};


struct action{
	int type;  /* forward, drop, log, send ...*/
	int p1_type;
	int p2_type;
	union {
		int number;
		char* string;
		void* data;
	}p1, p2;
	struct action* next;
};

struct expr* mk_exp(int op, struct expr* left, struct expr* right);
struct expr* mk_elem(int op, int subtype, int operand, void* param);
struct action* mk_action(int type, int p1_type, int p2_type, void* p1, void* p2);

void print_action(struct action* a);
void print_expr(struct expr* exp);





#endif

