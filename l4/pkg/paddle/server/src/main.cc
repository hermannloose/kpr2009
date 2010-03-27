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
	kbd = L4Re::Util::cap_alloc.alloc<void>();
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
			paddle->pressed(scancode);
			last_scancode = scancode;
		}
	}
}

Paddle::Paddle(int velocity, const int up_press, const int up_release, const int down_press, const int down_release)
: cxx::Thread(stack + sizeof(stack))
{
	l4_msgtag_t err;


	assert(velocity != 0);
	this->velocity = velocity;
	position = 0;
	direction = 0;

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

void Paddle::pressed(int scancode)
{

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

	timespec last_tick;
	timespec curr_tick;
	clock_gettime(CLOCK_REALTIME, &last_tick);
	clock_gettime(CLOCK_REALTIME, &curr_tick);
	double seconds = 0.0;

	int scancode = -1;
	int last_scancode = -1;

	int pressed = -1;

	while (1) {
		ios.reset();
		err = ios.call(kbd.cap(), 0);
		if (l4_ipc_error(err, l4_utcb())) {
			printf("Error while reading scancode!\n");
		}
		ios >> scancode;

		#if debug
		printf("Scancode %i received.\n", scancode);
		#endif

		if (scancode == up_press) position -= 10; 
		if (scancode == down_press) position += 10; 
			
		if (position < 0) position = 0;
		if (position > 1023) position = 1023;
		
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

	paddle->run();

  return 0;
}
