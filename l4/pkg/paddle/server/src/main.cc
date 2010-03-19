#include <l4/paddle/paddle.h>

#include <l4/cxx/ipc_stream>
#include <l4/re/env>
#include <l4/re/util/cap_alloc>

#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>

#define debug 1

Paddle::Paddle(int velocity, int up_press, int up_release, int down_press, int down_release)
: cxx::Thread(stack + sizeof(stack))
{
	l4_msgtag_t err;

	assert(velocity != 0);

	this->velocity = velocity;

	position = 0;

	this->up_press = up_press;
	this->up_release = up_release;
	this->down_press = down_press;
	this->down_release = down_release;

	printf("UP: %i / %i DOWN: %i / %i\n", up_press, up_release, down_press, down_release);

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

void Paddle::move(int pos)
{
	#if debug
	printf("Moving paddle to %i.\n", pos);
	#endif

	l4_msgtag_t err;

	L4::Ipc_iostream ios(l4_utcb());
	ios << 1UL << pos;

	if (l4_error(err = ios.call(paddle))) {
		printf("%04i: Failed to move paddle!\n", __LINE__);

		return;
	}

	position = pos;
}

void Paddle::run()
{
	move(0);

	L4::Ipc_iostream ios(l4_utcb());
	int scancode = -1;

	int moving = 0;

	while (1) {
		printf("Lives: %i\n", lives());
		// Get scancode.
		ios.reset();
		ios.call(kbd.cap(), 0);
		ios >> scancode;

		#if debug
		printf("Scancode %i received.\n", scancode);
		#endif

		if (scancode == up_press) {
			#if debug
			printf("Start moving up.\n");
			#endif
			if (moving == 0) moving = -1;
		}

		if (scancode == up_release) {
			#if debug
			printf("Stop moving up.\n");
			#endif
			if (moving == -1) moving = 0;
		}

		if (scancode == down_press) {
			#if debug
			printf("Start moving down.\n");
			#endif
			if (moving == 0) moving = 1;
		}

		if (scancode == down_release) {
			#if debug
			printf("Stop moving down.\n");
			#endif
			if (moving == 1) moving = 0;
		}

		#if debug
		printf("Moving: %i\n", moving);
		#endif

		int move_to = position + velocity * moving;
		if (move_to < 0) move_to = 0;
		if (move_to > 1023) move_to = 1023;
		move(move_to);

	}
}

int main(int argc, char **argv)
{
	if (argc != 6) {
		printf("Usage: paddle <velocity> <up_press> <up_release> <down_press> <down_release>\n");

		exit(1);
	}

	printf("Paddle starting.\n");
	// FIXME First cast is dirty.
	Paddle *paddle = new Paddle(atoi(argv[1]), atoi(argv[2]), atoi(argv[3]), atoi(argv[4]), atoi(argv[5]));
	paddle->run();

  return 0;
}
