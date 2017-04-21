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

struct Queue *new_queue(int max_size)
{
	struct Queue *queue = malloc(sizeof(struct Queue));
	queue->size = 0;
	queue->max_size = max_size;
	pthread_mutex_init(&queue->mutex, NULL);
	return queue;
}

bool queue_empty(struct Queue *queue)
{
	return queue->size == 0;
}

bool queue_full(struct Queue *queue)
{
	return queue->size == queue->max_size;
}

bool add_queue(struct Queue *queue, void *element)
{
	if (!queue_full(queue)) {
		struct Node *node = new_node(element, NULL);
		lock_queue(queue);
		if (queue_empty(queue)) {
			queue->first = node;
		} else {
			queue->last->next = node;
		}
		queue->last = node;
		queue->size++;
		unlock_queue(queue);
		return true;
	} else {
		return false;
	}
}

void *pop_queue(struct Queue *queue)
{
	struct Node *node = queue->first;
	void *element = node->element;
	lock_queue(queue);
	queue->first = node->next;
	free(node);
	queue->size--;
	unlock_queue(queue);
	return element;
}

void lock_queue(struct Queue *queue)
{
	pthread_mutex_lock(&(queue->mutex));
}

void unlock_queue(struct Queue *queue)
{
	pthread_mutex_unlock(&(queue->mutex));
}
