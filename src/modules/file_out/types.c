#include <stddef.h>
#include <stdlib.h>

#include "types.h"

Node *newNode(LogMessage data)
{
	Node *temp = (Node *)shm_malloc(sizeof(Node));
	temp->data = data;
	temp->next = NULL;
	return temp;
}


int enQueue(Queue *q, LogMessage data)
{
	/*
	Copy the contents of data.message
    */
	char *messageCopy = (char *)shm_malloc(strlen(data.message) + 1);
	strcpy(messageCopy, data.message);
	data.message = messageCopy;
	Node *temp = newNode(data);

	lock_get(&(q->lock));

	if(q->rear == NULL) {
		q->front = q->rear = temp;
		lock_release(&(q->lock));
		return 1;
	}

	q->rear->next = temp;
	q->rear = temp;

	lock_release(&(q->lock));
	return 1;
}

int deQueue(Queue *q, LogMessage *data)
{
	lock_get(&(q->lock));

	if(q->front == NULL) {
		lock_release(&(q->lock));
		return -1;
	}
	Node *temp = q->front;
	*data = temp->data;
	q->front = q->front->next;

	if(q->front == NULL)
		q->rear = NULL;


	if(temp != NULL) {
		if(temp->data.message != NULL) {
			shm_free(temp->data.message);
			temp->data.message = NULL;
		}
		shm_free(temp);
		temp = NULL;
	}
	lock_release(&(q->lock));

	return 1;
}

int isQueueEmpty(Queue *q)
{
	lock_get(&(q->lock));
	int result = (q->front == NULL);
	lock_release(&(q->lock));
	return result;
}

int queueSize(Queue *q)
{
	lock_get(&(q->lock));
	int count = 0;
	Node *temp = q->front;
	while(temp != NULL) {
		count++;
		temp = temp->next;
	}
	lock_release(&(q->lock));
	return count;
}
