/*
  Simple queue data structure by Michael Elliott
 */

#include "Queue.h"

struct Node *new_node(void *element, struct Node *next)
{
	struct Node *node = malloc(sizeof(struct Node));
	node->element = element;
	node->next = next;
	return node;
}

struct Queue *new_queue(void)
{
	return malloc(sizeof(struct Queue));
}

bool queue_empty(struct Queue *queue)
{
	return queue->first;
}

void add_queue(struct Queue *queue, void *element)
{
	struct Node *node = new_node(element, NULL);
	if (queue_empty(queue)) {
		queue->first = node;
	} else {
		queue->last->next = node;
	}
	queue->last = node;
}

void *pop_queue(struct Queue *queue)
{
	struct Node *node = queue->first;
	void *element = node->element;
	queue->first = node->next;
	free(node);
	return element;
}

