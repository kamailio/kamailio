/*
 * $Id$
 *
 * route structures helping functions
 */


#include  "route_struct.h"

#include <stdio.h>
#include <stdlib.h>
#include <netinet/in.h>

struct expr* mk_exp(int op, struct expr* left, struct expr* right)
{
	struct expr * e;
	e=(struct expr*)malloc(sizeof (struct expr));
	if (e==0) goto error;
	e->type=EXP_T;
	e->op=op;
	e->l.expr=left;
	e->r.expr=right;
	return e;
error:
	fprintf(stderr, "ERROR: mk_exp: memory allocation failure\n");
	return 0;
}


struct expr* mk_elem(int op, int subtype, int operand, void* param)
{
	struct expr * e;
	e=(struct expr*)malloc(sizeof (struct expr));
	if (e==0) goto error;
	e->type=ELEM_T;
	e->op=op;
	e->subtype=subtype;
	e->l.operand=operand;
	e->r.param=param;
	return e;
error:
	fprintf(stderr, "ERROR: mk_elem: memory allocation failure\n");
	return 0;
}



struct action* mk_action(int type, int p1_type, int p2_type, void* p1, void* p2)
{
	struct action* a;
	a=(struct action*)malloc(sizeof(struct action));
	if (a==0) goto  error;
	a->type=type;
	a->p1_type=p1_type;
	a->p2_type=p2_type;
	a->p1.string=(char*) p1;
	a->p2.string=(char*) p2;
	a->next=0;
	return a;
	
error:
	fprintf(stderr, "ERROR: mk_action: memory allocation failure\n");
	return 0;

}


struct action* append_action(struct action* a, struct action* b)
{
	struct action *t;
	if (b==0) return a;
	if (a==0) return b;
	
	for(t=a;t->next;t=t->next);
	t->next=b;
	return a;
}



struct net* mk_net(unsigned long ip, unsigned long mask)
{
	struct net* n;

	n=(struct net*)malloc(sizeof(struct net));
	if (n==0) goto error;
	n->ip=ip;
	n->mask=mask;
	return n;
error:
	fprintf(stderr, "ERROR: mk_net_mask: memory allocation failure\n");
	return 0;
}

	
	

void print_ip(unsigned ip)
{
	printf("%d.%d.%d.%d", ((unsigned char*)&ip)[0],
						  ((unsigned char*)&ip)[1],
						  ((unsigned char*)&ip)[2],
						  ((unsigned char*)&ip)[3]);
}


void print_net(struct net* net)
{
	if (net==0){
		fprintf(stderr, "ERROR: print net: null pointer\n");
		return;
	}
	print_ip(net->ip); printf("/"); print_ip(net->mask);
}



void print_expr(struct expr* exp)
{
	if (exp==0){
		fprintf(stderr, "ERROR: print_expr: null expression!\n");
		return;
	}
	if (exp->type==ELEM_T){
		switch(exp->l.operand){
			case METHOD_O:
				printf("method");
				break;
			case URI_O:
				printf("uri");
				break;
			case SRCIP_O:
				printf("srcip");
				break;
			case DSTIP_O:
				printf("dstip");
				break;
			default:
				printf("UNKNOWN");
		}
		switch(exp->op){
			case EQUAL_OP:
				printf("==");
				break;
			case MATCH_OP:
				printf("~=");
				break;
			default:
				printf("<UNKNOWN>");
		}
		switch(exp->subtype){
			case NOSUBTYPE: 
					printf("N/A");
					break;
			case STRING_ST:
					printf("\"%s\"", (char*)exp->r.param);
					break;
			case NET_ST:
					print_net((struct net*)exp->r.param);
					break;
			case IP_ST:
					print_ip(exp->r.intval);
					break;
			default:
					printf("type<%d>", exp->subtype);
		}
	}else if (exp->type==EXP_T){
		switch(exp->op){
			case AND_OP:
					printf("AND( ");
					print_expr(exp->l.expr);
					printf(", ");
					print_expr(exp->r.expr);
					printf(" )");
					break;
			case OR_OP:
					printf("OR( ");
					print_expr(exp->l.expr);
					printf(", ");
					print_expr(exp->r.expr);
					printf(" )");
					break;
			case NOT_OP:	
					printf("NOT( ");
					print_expr(exp->l.expr);
					printf(" )");
					break;
			default:
					printf("UNKNOWN_EXP ");
		}
					
	}else{
		printf("ERROR:print_expr: unknown type\n");
	}
}
					

					

void print_action(struct action* a)
{
	struct action* t;
	for(t=a; t!=0;t=t->next){
		switch(t->type){
			case FORWARD_T:
					printf("forward(");
					break;
			case SEND_T:
					printf("send(");
					break;
			case DROP_T:
					printf("drop(");
					break;
			case LOG_T:
					printf("log(");
					break;
			case ERROR_T:
					printf("error(");
					break;
			case ROUTE_T:
					printf("route(");
					break;
			case EXEC_T:
					printf("exec(");
					break;
			default:
					printf("UNKNOWN(");
		}
		switch(t->p1_type){
			case STRING_ST:
					printf("\"%s\"", t->p1.string);
					break;
			case NUMBER_ST:
					printf("%d",t->p1.number);
					break;
			case IP_ST:
					print_ip(t->p1.number);
					break;
			default:
					printf("type<%d>", t->p1_type);
		}
		switch(t->p2_type){
			case NOSUBTYPE:
					break;
			case STRING_ST:
					printf(", \"%s\"", t->p2.string);
					break;
			case NUMBER_ST:
					printf(", %d",t->p2.number);
					break;
			default:
					printf(", type<%d>", t->p2_type);
		}
		printf("); ");
	}
}
			
	

	
	

