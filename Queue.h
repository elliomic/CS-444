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
	pthread_mutex_t mutex;
};

struct Node *new_node(void *, struct Node *);

struct Queue *new_queue(int);

bool queue_empty(struct Queue *);

bool queue_full(struct Queue *);

bool add_queue(struct Queue *, void *);

void *pop_queue(struct Queue *);

void lock_queue(struct Queue *);

void unlock_queue(struct Queue *);

#endif
