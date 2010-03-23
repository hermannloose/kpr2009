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

sem_t paddle_started;

Input::Input(Paddle *parent, int up_press, int up_release, int down_press, int down_release)
: cxx::Thread(stack + sizeof(stack))
{
	this->parent = parent;

}

void Input::run()
{
	#if debug
	printf("%p: Input thread starting.\n", pthread_self());
	#endif

	L4::Ipc_iostream ios(l4_utcb());
	int scancode = 0;

	sem_wait(&paddle_started);

	#if debug
	printf("Processing input ...\n");
	#endif

	while (1) {
		ios.reset();
		ios.call(kbd.cap(), 0);
		ios >> scancode;

		if (scancode == up_press) parent->move(-1);
		if (scancode == up_release) parent->move(0);
		if (scancode == down_press) parent->move(1);
		if (scancode == down_release) parent->move(0);

		sleep(1);
	}
}

Paddle::Paddle(int velocity, const int up_press, const int up_release, const int down_press, const int down_release)
: cxx::Thread(stack + sizeof(stack))
{
/*	Input *input = new Input(this, up_press, up_release, down_press, down_release);
	
	#if debug
	printf("Input thread created.\n");
	#endif

	input->start();

	#if debug
	printf("Input thread started.\n");
	#endif

	sem_init(&mutex, 0, 1);
*/
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

void Paddle::move(int direction)
{
	assert(-1 <= direction && direction <= 1);
	sem_wait(&mutex);
	this->direction = direction;
	sem_post(&mutex);
}

void Paddle::run()
{
	l4_msgtag_t err;

	L4::Ipc_iostream ios(l4_utcb());

	clock_t last_tick = clock();
	clock_t curr_tick = clock();
	double seconds = 0.0;

	int scancode;

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

		if (scancode == up_press) direction = -1;
		if (scancode == up_release) direction = 0;
		if (scancode == down_press) direction = -1;
		if (scancode == down_release) direction = 0;

		curr_tick = clock();
		seconds = (curr_tick - last_tick) / CLOCKS_PER_SEC;

		#if debug
		printf("%i last tick\n%i current tick\n%d seconds since last iteration.\n", last_tick, curr_tick, seconds);
		#endif

		position += (velocity * direction * seconds);
		if (position < 0) position = 0;
		if (position > 1023) position = 1023;
		
		ios.reset();
		ios << 1UL << position;
		ios.call(paddle);
		// FIXME Error handling

		last_tick = curr_tick;

		sleep(0.2);
	}
}

int main(int argc, char **argv)
{
	if (argc != 6) {
		printf("Usage: paddle <velocity> <up_press> <up_release> <down_press> <down_release>\n");

		exit(1);
	}

	printf("Paddle starting.\n");
	
	sem_init(&paddle_started, 0, 0);

	// FIXME First cast is dirty.
	Paddle *paddle = new Paddle(atoi(argv[1]), atoi(argv[2]), atoi(argv[3]), atoi(argv[4]), atoi(argv[5]));

	sem_post(&paddle_started);

	paddle->run();

  return 0;
}
