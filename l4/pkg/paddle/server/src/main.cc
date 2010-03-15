#include <l4/paddle/paddle.h>

#include <l4/re/util/cap_alloc>

#include <assert.h>
#include <stdio.h>
#include <stdlib.h>

Paddle::Paddle(int velocity, char up_press, char up_release, char down_press, char down_release)
: cxx::Thread(stack + sizeof(stack))
{
	assert(velocity != 0);

	this->velocity = velocity;
	this->up_press = up_press;
	this->up_release = up_release;
	this->down_press = down_press;
	this->down_release = down_release;

	kbd = L4Re::Util::cap_alloc.alloc<void>();
	if (!kbd.is_valid()) {
		printf("Could not get a valid capability slot!\n");

		exit(1);
	}
}

void Paddle::run()
{
	while (1) {

	}
}

int main(int argc, char **argv)
{
  return 0;
}
