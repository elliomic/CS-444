/*Michael Elliott, Kirash Teymoury, Liv Vitale*/

#include <stdio.h>
#include <stdlib.h>
#include <stdbool.h>
#include <pthread.h>
#include <semaphore.h>
#include <unistd.h>

#define TOBACCO 0
#define PAPER 1
#define MATCH 2

typedef int ingredient_t;

struct Table {
    sem_t agent_sem;
    sem_t pusher_sems[3];
    sem_t smoker_sems[3];
    bool ingredients[3];
    pthread_mutex_t table_lock;
};

struct TableIngredient {
    struct Table *table;
    ingredient_t ing;
};

int rand_range(int min, int max)
{
	return rand() % (max - min + 1) + min;
}

ingredient_t rand_ingredient()
{
    return rand_range(TOBACCO, MATCH);
}

void *agent_thread_routine(void *table_ptr)
{
    struct Table *table = (struct Table *)table_ptr;
    ingredient_t ing1, ing2;
	while (true) {
        sem_wait(&table->agent_sem);
        ing1 = rand_ingredient();
        do {
            ing2 = rand_ingredient();
        } while (ing1 == ing2);
        sem_post(&table->pusher_sems[ing1]);
        sem_post(&table->pusher_sems[ing2]);
        sleep(1);
	}
	
	return 0;
}

void *pusher_thread_routine(void *pusher_ptr)
{
    struct TableIngredient *ti = (struct TableIngredient *)pusher_ptr;
    ingredient_t ing1, ing2;
    char *ing_name;

    if (ti->ing == TOBACCO) {
        ing1 = PAPER;
        ing2 = MATCH;
        ing_name = "TOBACCO";
    } else if (ti->ing == PAPER) {
        ing1 = TOBACCO;
        ing2 = MATCH;
        ing_name = "PAPER";
    } else if (ti->ing == MATCH) {
        ing1 = PAPER;
        ing2 = TOBACCO;
        ing_name = "a MATCH";
    }
    
	while (true) {
        sem_wait(&ti->table->pusher_sems[ti->ing]);
        pthread_mutex_lock(&ti->table->table_lock);
        printf("Agent is adding %s to the table\n", ing_name);
        if (ti->table->ingredients[ing1]) {
            ti->table->ingredients[ing1] = false;
            sem_post(&ti->table->smoker_sems[ing2]);
        } else if (ti->table->ingredients[ing2]) {
            ti->table->ingredients[ing2] = false;
            sem_post(&ti->table->smoker_sems[ing1]);
        } else {
            ti->table->ingredients[ti->ing] = true;
        }
        pthread_mutex_unlock(&ti->table->table_lock);
        sleep(1);
	}
	
	return 0;	
}

void *smoker_thread_routine(void *smoker_ptr)
{
    struct TableIngredient *ti = (struct TableIngredient *)smoker_ptr;
	while (true) {
        sem_wait(&ti->table->smoker_sems[ti->ing]);
        printf("Smoker %d is making a cigarette\n", ti->ing);
        sleep(1);
        sem_post(&ti->table->agent_sem);
        printf("Smoker %d is smoking their cigarette\n", ti->ing);
        sleep(1);
	}
	
	return 0;
}

struct Table *new_table()
{
    struct Table *table = malloc(sizeof(struct Table));
    int i;
    sem_init(&table->agent_sem, 0, 1);
    for(i = 0; i < 3; i++) {
        sem_init(&table->pusher_sems[i], 0, 0);
        sem_init(&table->smoker_sems[i], 0, 0);
    }
    pthread_mutex_init(&table->table_lock, NULL);

    return table;
}

struct TableIngredient *new_ingredient(struct Table *table, ingredient_t ing)
{
    struct TableIngredient *ti = malloc(sizeof(struct TableIngredient));
    ti->table = table;
    ti->ing = ing;
    return ti;
}

int main()
{
	pthread_t threads[7];
	int j;

	struct Table *table = new_table();
    struct TableIngredient *tobacco = new_ingredient(table, TOBACCO);
    struct TableIngredient *paper = new_ingredient(table, PAPER);
    struct TableIngredient *match = new_ingredient(table, MATCH);

    pthread_create(&threads[0], 0, agent_thread_routine, table);
    pthread_create(&threads[1], 0, pusher_thread_routine, tobacco);
    pthread_create(&threads[2], 0, pusher_thread_routine, paper);
    pthread_create(&threads[3], 0, pusher_thread_routine, match);
    pthread_create(&threads[4], 0, smoker_thread_routine, tobacco);
    pthread_create(&threads[5], 0, smoker_thread_routine, paper);
    pthread_create(&threads[6], 0, smoker_thread_routine, match);

	for (j = 0; j < 7; j++) {
		pthread_join(threads[j], NULL);
	}
	
	return 0;
}
