/*Michael Elliott, Kirash Teymoury, Liv Vitale*/

#include <stdio.h>
#include <stdlib.h>
#include <stdbool.h>
#include <pthread.h>
#include <semaphore.h>
#include <unistd.h>
#include <alloca.h>

struct philosopher {
	int id;
	pthread_t thread;
	pthread_mutex_t *left_fork;
	pthread_mutex_t *right_fork;
	sem_t *semaphore;
};

int rand_range(int min, int max)
{
	return rand() % (max - min + 1) + min;
}

void get_forks(struct philosopher *philosopher)
{
	pthread_mutex_lock(philosopher->left_fork);
	pthread_mutex_lock(philosopher->right_fork);
}

void put_forks(struct philosopher *philosopher)
{
	pthread_mutex_unlock(philosopher->left_fork);
	pthread_mutex_unlock(philosopher->right_fork);
}

void *philosopher_thread_routine(void *phil_ptr)
{
	struct philosopher *philosopher = phil_ptr;
	
	while (true) {
		printf("philosopher %d is thinking\n", philosopher->id);
		sleep(rand_range(1, 20));
		sem_wait(philosopher->semaphore);
		get_forks(philosopher);
		printf("philosopher %d is eating\n", philosopher->id);
		sleep(rand_range(2, 9));
		put_forks(philosopher);
		sem_post(philosopher->semaphore);
	}
	
	return 0;
}

void spawn_threads(const int n)
{
	struct philosopher *philosophers;
	pthread_mutex_t *forks;
	sem_t *semaphore;
	int i;

	philosophers = alloca(sizeof(struct philosopher) * n);
	forks = alloca(sizeof(pthread_mutex_t) * n);
	semaphore = alloca(sizeof(sem_t));
	sem_init(semaphore, 0, n - 1);
	for (i = 0; i < n; i++) {
		pthread_mutex_init(&forks[i], NULL);
	}
	for (i = 0; i < n; i++) {
		philosophers[i].id = i;
		pthread_create(&philosophers[i].thread, NULL,
		               philosopher_thread_routine, &philosophers[i]);
		philosophers[i].left_fork = &forks[i];
		if (i < n - 1) {
			philosophers[i].right_fork = &forks[i + 1];
		} else {
			philosophers[i].right_fork = &forks[0];
		}
		philosophers[i].semaphore = semaphore;
	}
	for (i = 0; i < n; i++) {
		pthread_join(philosophers[i].thread, NULL);
	}
}

int main(int argc, char **argv)
{
	if (argc != 2) {
		fprintf(stderr,
		        "USAGE: %s <num_philosophers>\n",
		        argv[0]);
		return 1;
	} else {
		int n = atoi(argv[1]);

		if (n > 0) {
			spawn_threads(n);
		} else {
			fprintf(stderr,
			        "error: <num_philosophers> must be > 0\n");
			return 1;
		}
	}

	return 0;
}

