/*
  Simple locking queue data structure by Michael Elliott
 */

#ifndef QUEUE_H
#define QUEUE_H

#include <stdlib.h>
#include <stdbool.h>
#include <pthread.h>

struct Node {
	void *element;
	struct Node *next;
};

struct Queue {
	struct Node *first;
	struct Node *last;
	int size;
	int max_size;
	int search_sem;
	pthread_mutex_t insert_lock;
	pthread_mutex_t delete_lock;
};

struct Node *new_node(void *, struct Node *);

struct Queue *new_queue(int);

bool queue_empty(struct Queue *);

bool queue_full(struct Queue *);

bool add_queue(struct Queue *, void *);

void *pop_queue(struct Queue *);

void lock_search(struct Queue *);

void lock_insert(struct Queue *);

void lock_delete(struct Queue *);

void unlock_search(struct Queue *);

void unlock_insert(struct Queue *);

void unlock_delete(struct Queue *);

#endif

