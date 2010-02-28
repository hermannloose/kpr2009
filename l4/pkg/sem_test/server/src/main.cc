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

void *run(void *arg)
{
	int iteration = 0;
	while(1)
	{
		printf("%x / %i: pre-down\n", pthread_self(), iteration);
		//enter_kdebug("pre-down");
		//sem->down();
		sem_wait(&sem);
		//enter_kdebug("post-down");
		printf("%x / %i: critical\n", pthread_self(), iteration);
		//enter_kdebug("pre-up");
		//sem->up();
		sem_post(&sem);
		//enter_kdebug("post-up");
		printf("%x / %i: post-up\n", pthread_self(), iteration);
		iteration++;
		/*l4_timeout_t timeout;
		timeout.p.rcv.t = rand() % 2000;
		l4_ipc_sleep(timeout);*/
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
	sem_init(&sem, 0, 1);
	pthread_t t1, t2;
	pthread_create(&t1, NULL, &run, NULL);
	pthread_create(&t2, NULL, &run, NULL);
	while (1);
	pthread_join(t1, NULL);
	pthread_join(t2, NULL);
}

