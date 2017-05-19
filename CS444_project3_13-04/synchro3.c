/*Michael Elliott, Kirash Teymoury, Liv Vitale*/

#include <stdio.h>
#include <string.h>
#include <stdbool.h>
#include <pthread.h>
#include <unistd.h>
#include <alloca.h>
#include "Queue.h"

void *searcher_thread_routine(void *buffer_ptr)
{
	struct Queue *buffer = (struct Queue *)buffer_ptr;

	while (true) {
		lock_search(buffer);
		puts("Searching...");
		sleep(1);
		puts("Done Searching");
		unlock_search(buffer);
		sleep(1);
	}
	
	return 0;
}

void *inserter_thread_routine(void *buffer_ptr)
{
	struct Queue *buffer = (struct Queue *)buffer_ptr;

	while (true) {
		lock_insert(buffer);
		if (!queue_full(buffer)) {
			puts("Inserting...");
			add_queue(buffer, NULL);
			sleep(1);
			puts("Done Inserting");
		}
		unlock_insert(buffer);
		sleep(1);
	}
	
	return 0;
}

void *deleter_thread_routine(void *buffer_ptr)
{
	struct Queue *buffer = (struct Queue *)buffer_ptr;

	while (true) {
		lock_delete(buffer);
		if (!queue_empty(buffer)) {
			puts("Deleting...");
			pop_queue(buffer);
			sleep(1);
			puts("Done Deleting");
		}
		unlock_delete(buffer);
		sleep(1);
	}
	
	return 0;	
}

void spawn_threads(const int s, const int i, const int d)
{
	struct Queue *buffer;
	pthread_t *searchers;
	pthread_t *inserters;
	pthread_t *deleters;
	int j;

	buffer = new_queue(-1);
	searchers = alloca(sizeof(pthread_t) * s);
	inserters = alloca(sizeof(pthread_t) * i);
	deleters = alloca(sizeof(pthread_t) * d);

	for (j = 0; j < s; j++) {
		pthread_create(&searchers[i], 0, searcher_thread_routine, buffer);
	}
	for (j = 0; j < i; j++) {
		pthread_create(&inserters[i], 0, inserter_thread_routine, buffer);
	}
	for (j = 0; j < d; j++) {
		pthread_create(&deleters[i], 0, deleter_thread_routine, buffer);
	}
	for (j = 0; j < s; j++) {
		pthread_join(searchers[i], NULL);
	}
	for (j = 0; j < i; j++) {
		pthread_join(inserters[i], NULL);
	}
	for (j = 0; j < d; j++) {
		pthread_join(deleters[i], NULL);
	}
}

int main(int argc, char **argv)
{
	if (argc != 4) {
		fprintf(stderr,
		        "USAGE: %s <num_searchers> <num_inserters> <num_deleters>\n",
		        argv[0]);
		return 1;
	} else {
		int s = atoi(argv[1]);
		int i = atoi(argv[2]);
		int d = atoi(argv[3]);

		if (s >= 0 && i >= 0 && d >= 0) {
			spawn_threads(s, i, d);
		} else {
			fprintf(stderr,
			        "error: <num_searchers>, <num_inserters>,"
			        " and <num_deleters> must be >= 0\n");
			return 1;
		}
	}

	return 0;
}

