#include "../../core/locking.h"

typedef struct log_message
{
	char *message;
	int dest_file;
} LogMessage;

typedef struct node
{
	struct log_message data;
	struct node *next;
} Node;

typedef struct queue
{
	struct node *front;
	struct node *rear;
	gen_lock_t lock;
} Queue;

int enQueue(Queue *q, LogMessage data);
int deQueue(Queue *q, LogMessage *data);
int isQueueEmpty(Queue *q);
int queueSize(Queue *q);
