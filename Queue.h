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
};

struct Node *new_node(void *, struct Node *);

struct Queue *new_queue(void);

bool queue_empty(struct Queue *);

void add_queue(struct Queue *, void *);

void *pop_queue(struct Queue *);

#endif
