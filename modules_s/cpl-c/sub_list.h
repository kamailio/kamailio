#ifndef _CPL_SUB_LIST_H
#define _CPL_SUB_LIST_H

struct node {
	unsigned char  *offset;
	char           *name;
	struct node    *next;
};


struct node*   append_to_list(struct node *head, unsigned char *offdet,
																char *name);
unsigned char* search_the_list(struct node *head, char *name);
void           delete_list(struct node *head );

#endif
