#include <stdlib.h>
#include <string.h>
#include "sub_list.h"

struct node*   append_to_list(struct node *head, unsigned char *offset,
																char *name)
{
	struct node *n;
	struct node *new_node;

	new_node = malloc(sizeof(struct node));
	if (!new_node)
		return 0;
	new_node->offset = offset;
	new_node->name = name;
	new_node->next = 0;
	if (head) {
		n = head;
		while (n->next)
			n = n->next;
		n->next = new_node;
	}

	return new_node;
}




unsigned char* search_the_list(struct node *head, char *name)
{
	struct node *n;

	n = head;
	while (n) {
		if (strcasecmp(n->name,name)==0)
			return n->offset;
		n = n->next;
	}
	return 0;
}




void delete_list(struct node* head)
{
	struct node *n;
;
	while (head) {
		n=head->next;
		free(head);
		head = n;
	}
}


