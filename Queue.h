#ifndef QUEUE_H
#define QUEUE_H

#include <stdlib.h>
#include <stdbool.h>

struct Node {
	void *element;
	struct Node *next;
};

struct Queue {
	struct Node *first;
	struct Node *last;
	int size;
	int max_size;
};

struct Node *new_node(void *, struct Node *);

struct Queue *new_queue(int);

bool queue_empty(struct Queue *);

bool add_queue(struct Queue *, void *);

void *pop_queue(struct Queue *);

#endif
