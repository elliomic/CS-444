/*
  Simple locking queue data structure by Michael Elliott
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
	queue->search_sem = 0;
	pthread_mutex_init(&queue->insert_lock, NULL);
	pthread_mutex_init(&queue->delete_lock, NULL);
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
		if (queue_empty(queue)) {
			queue->first = node;
		} else {
			queue->last->next = node;
		}
		queue->last = node;
		queue->size++;
		return true;
	} else {
		return false;
	}
}

void *pop_queue(struct Queue *queue)
{
	void *element;
	if (!queue_empty(queue)) {
		struct Node *node = queue->first;
		element = node->element;
		queue->first = node->next;
		free(node);
		queue->size--;
	}
	return element;
}

void lock_search(struct Queue *queue)
{
	if (!queue->search_sem) {
		lock_delete(queue);
	}
	queue->search_sem++;
}

void lock_insert(struct Queue *queue)
{
	lock_search(queue);
	pthread_mutex_lock(&(queue->insert_lock));
}

void lock_delete(struct Queue *queue)
{
	pthread_mutex_lock(&(queue->delete_lock));
}

void unlock_search(struct Queue *queue)
{
	queue->search_sem--;
	if (!queue->search_sem) {
		unlock_delete(queue);
	}
}

void unlock_insert(struct Queue *queue)
{
	pthread_mutex_unlock(&(queue->insert_lock));
	unlock_search(queue);
}

void unlock_delete(struct Queue *queue)
{
	pthread_mutex_unlock(&(queue->delete_lock));
}
