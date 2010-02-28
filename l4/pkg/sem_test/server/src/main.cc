#include <l4/re/env>
#include <l4/re/util/cap_alloc>
#include <l4/re/util/object_registry>
#include <l4/sys/capability>
#include <l4/sys/ipc.h>
#include <l4/sys/kdebug.h>
//#include <l4/sys/semaphore>
#include <pthread.h>
#include <semaphore.h>
#include <stdio.h>
#include <stdlib.h>

//static L4::Semaphore *sem;
sem_t sem;

void *produce(void *arg)
{
	int iteration = 1;
	while (1) {
		printf("Produce: %i\n", iteration++);
		sem_post(&sem);
	}
	return NULL;
}

void *consume(void *arg)
{
	int iteration = 0;
	while (1) {
		sem_wait(&sem);
		printf("Consume: %i\n", iteration++);
	}
	return NULL;
}

int main(int argc, char **argv)
{
  /*L4::Cap<L4::K_semaphore> ksem;
	if (l4_error(L4Re::Env::env()->factory()->create_semaphore(ksem))) {
		printf("Error creating semaphore!\n");
	}
	sem = new L4::Semaphore(ksem);
	sem->init(1);*/
	sem_init(&sem, 0, 0);
	pthread_t t1, t2;
	pthread_create(&t1, NULL, &produce, NULL);
	pthread_create(&t2, NULL, &consume, NULL);
	pthread_join(t1, NULL);
	pthread_join(t2, NULL);
}

