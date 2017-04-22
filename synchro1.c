/*Michael Elliot, Kirash Teymory, Liv Vitale*/

#include <stdio.h>
#include <string.h>
#include <stdbool.h>
#include <pthread.h>
#include <unistd.h>
#include <alloca.h>
#include "mt19937ar.h"
#include "Queue.h"

#define BUFFER_SIZE 32

struct buffer_item {
	int number;
	int work;
};

unsigned int CPUID;

void set_cpuid()
{
	__asm__ __volatile__(
		                 "cpuid;"
		                 : "=c"(CPUID)
		                 : "a"(0x01)
		                 );
}

bool rdrand_supported()
{
	return CPUID & 0x40000000;
}

int rand()
{
	if (rdrand_supported()) {
		int result;
		__asm__ __volatile__(
			                 "rdrand %0"
			                 :"=r"(result)
			                 );
		return result;
	} else {
		return genrand_int32();
	}
}

int rand_range(int min, int max)
{
	return (unsigned int)rand() % (max - min + 1) + min;
}

struct buffer_item *new_buffer_item()
{
	struct buffer_item *item = malloc(sizeof(struct buffer_item));
	item->number = rand();
	item->work = rand_range(2, 9);
	return item;
}

void *producer_thread_routine(void *buffer_ptr)
{
	struct Queue *buffer = (struct Queue *)buffer_ptr;

	while (true) {
		lock_queue(buffer);
		if (!queue_full(buffer)) {
			add_queue(buffer, new_buffer_item());
			unlock_queue(buffer);
			sleep(rand_range(3, 7));
		} else {
			unlock_queue(buffer);
		}
	}
	
	return 0;
}

void *consumer_thread_routine(void *buffer_ptr)
{
	struct Queue *buffer = (struct Queue *)buffer_ptr;

	while (true) {
		lock_queue(buffer);
		if (!queue_empty(buffer)) {
			struct buffer_item *item = (struct buffer_item *)pop_queue(buffer);
			unlock_queue(buffer);
			printf("%d\n", item->number);
			sleep(item->work);
		} else {
			unlock_queue(buffer);
		}
	}
	
	return 0;
}

void spawn_threads(const int p, const int c)
{
	struct Queue *buffer;
	pthread_t *producers;
	pthread_t *consumers;
	int i;

	buffer = new_queue(BUFFER_SIZE);
	producers = alloca(sizeof(pthread_t) * p);
	consumers = alloca(sizeof(pthread_t) * c);

	for (i = 0; i < p; i++) {
		pthread_create(&producers[i], 0, producer_thread_routine, buffer);
	}
	for (i = 0; i < c; i++) {
		pthread_create(&consumers[i], 0, consumer_thread_routine, buffer);
	}
	for (i = 0; i < p; i++) {
		pthread_join(producers[i], NULL);
	}
	for (i = 0; i < p; i++) {
		pthread_join(consumers[i], NULL);
	}
}

int main(int argc, char **argv)
{
	if (argc != 3) {
		fprintf(stderr,
		        "USAGE: %s <num_producers> <num_consumers>\n",
		        argv[0]);
		return 1;
	} else {
		int p = atoi(argv[1]);
		int c = atoi(argv[2]);

		if (p > 0 && c > 0) {
			set_cpuid();
			spawn_threads(p, c);
		} else {
			fprintf(stderr,
			        "error: <num_producers> and <num_consumers>"
			        " must be > 0\n");
			return 1;
		}
	}

	return 0;
}

