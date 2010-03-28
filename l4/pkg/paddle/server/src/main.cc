#include <l4/paddle/paddle.h>

#include <l4/cxx/ipc_stream>
#include <l4/re/env>
#include <l4/re/util/cap_alloc>

#include <assert.h>
#include <pthread.h>
#include <stdio.h>
#include <stdlib.h>
#include <time.h>
#include <unistd.h>

#define debug 1

Paddle *paddle;

void * kbd_consume(void *args)
{
	// Get keyboard driver.
	L4::Cap<void> kbd = L4Re::Util::cap_alloc.alloc<void>();
	if (!kbd.is_valid()) {
		printf("%04i: Could not get a valid capability slot!\n", __LINE__);

		exit(1);
	}
	if (L4Re::Env::env()->names()->query("kbd", kbd)) {
		printf("%04i: Could not get 'kbd' capability!\n", __LINE__);

		exit(1);
	}
	
	L4::Ipc_iostream ios(l4_utcb());
	int scancode = -1;
	int last_scancode = -1;

	while (1) {
		ios.reset();
		ios.call(kbd.cap(), 0);
		ios >> scancode;
		if (scancode != last_scancode) {
			paddle->press(scancode);
			last_scancode = scancode;
		}
	}
}

Paddle::Paddle(int velocity, const int up_press, const int up_release, const int down_press, const int down_release)
{
	l4_msgtag_t err;

	assert(velocity != 0);
	this->velocity = velocity;
	position = 0;

	direction = 0;
	sem_init(&mtx_direction, 0, 1);

	pressed = -1;
	sem_init(&mtx_pressed, 0, 1);

	#if debug
	printf("UP: %i / %i DOWN: %i / %i\n", up_press, up_release, down_press, down_release);
	#endif

	this->up_press = up_press;
	this->up_release = up_release;
	this->down_press = down_press;
	this->down_release = down_release;


	svr = L4Re::Util::cap_alloc.alloc<void>();
	if (!svr.is_valid()) {
		printf("%04i: Could not get a valid capability slot!\n", __LINE__);

		exit(1);
	}
	if (L4Re::Env::env()->names()->query("svr", svr)) {
		printf("%04i: Could not get 'svr' capability!\n", __LINE__);

		exit(1);
	}

	L4::Ipc_iostream ios(l4_utcb());
	ios << 1UL;
	ios << L4::Small_buf(paddle);
	err = ios.call(svr.cap());

	// FIXME Proper error handling needed.
	if (l4_error(err)) {
		printf("Could not connect to pong server!\n");

		exit(1);
	}

}

int Paddle::lives()
{
	l4_msgtag_t err;

	L4::Ipc_iostream ios(l4_utcb());
	ios << 3UL;

	if (l4_error(err = ios.call(paddle))) {
		printf("%04i: Failed to query server for score!\n", __LINE__);

		return -1;
	}

	int lives;
	ios >> lives;
	
	return lives;
}

void Paddle::press(int scancode)
{
	sem_wait(&mtx_pressed);
	this->pressed = scancode;
	sem_post(&mtx_pressed);
}

void Paddle::move(int direction)
{
	sem_wait(&mtx_direction);
	this->direction = direction;
	sem_post(&mtx_direction);
}

void Paddle::run()
{
	l4_msgtag_t err;

	L4::Ipc_iostream ios(l4_utcb());
/*
	timespec last_tick;
	timespec curr_tick;
	clock_gettime(CLOCK_REALTIME, &last_tick);
	clock_gettime(CLOCK_REALTIME, &curr_tick);
	double seconds = 0.0;
*/
	while (1) {

		if (pressed == up_press) position -= velocity; 
		if (pressed == down_press) position += velocity; 
			
		if (position < 0) {
			position = 0;
			printf("Paddle hit upper border!\n");
		}
		if (position > 1023) {
			position = 1023;
			printf("Paddle hit lower border!\n");
		}
		
		ios.reset();
		ios << 1UL << position;
		ios.call(paddle);
		// FIXME Error handling

		usleep(50000);
	}
}

int main(int argc, char **argv)
{
	if (argc != 6) {
		printf("Usage: paddle <velocity> <up_press> <up_release> <down_press> <down_release>\n");

		exit(1);
	}

	printf("Paddle starting.\n");
	
	paddle = new Paddle(atoi(argv[1]), atoi(argv[2]), atoi(argv[3]), atoi(argv[4]), atoi(argv[5]));

	pthread_t consumer;
	pthread_create(&consumer, NULL, kbd_consume, NULL);

	#if debug
	printf("Paddle created.\n");
	#endif

	paddle->run();

  return 0;
}
