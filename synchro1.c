#include <stdio.h>
#include <string.h>
#include <stdbool.h>
#include <pthread.h>
#include <unistd.h>
#include "mt19937ar.h"
#include "Queue.h"

struct buffer_item {
	int number;
	int work;
};

unsigned int CPUID;

void set_cpuid()
{
	__asm__ __volatile__(
		                 "cpuid;"
		                 :"=c"(CPUID)
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
	return rand() % (max - min + 1) + min;
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
		if (queue_full(buffer)) {
			continue;
		} else {
			struct buffer_item *item = new_buffer_item();
			lock_queue(buffer);
			add_queue(buffer, item);
			unlock_queue(buffer);
			sleep(rand_range(3, 7));
		}
	}
	
	return 0;
}

void *consumer_thread_routine(void *buffer_ptr)
{
	struct Queue *buffer = (struct Queue *)buffer_ptr;

	while (true) {
		if (queue_empty(buffer)) {
			continue;
		} else {
			struct buffer_item *item;
			lock_queue(buffer);
			item = (struct buffer_item *)pop_queue(buffer);
			unlock_queue(buffer);
			printf("%d\n", item->number);
			sleep(item->work);
		}
	}
	
	return 0;
}

int main()
{
	struct Queue *buffer;

	set_cpuid();
	buffer = new_queue(32);

	return 0;
}
