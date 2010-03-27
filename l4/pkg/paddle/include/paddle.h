#ifndef PADDLE_H
#define PADDLE_H

#include <l4/cxx/thread.h>

#include <semaphore.h>

enum {
	UP_PRESS, UP_RELEASE, DOWN_PRESS, DOWN_RELEASE
};

enum {
	NEUTRAL, UP, DOWN
};

class Paddle : public cxx::Thread
{
	private:
		int up_press, up_release;
		int down_press, down_release;
		int position;
		int velocity;
		int direction;
		L4::Cap<void> kbd;
		L4::Cap<void> svr;
		L4::Cap<void> console;
		unsigned long paddle;	

		char stack[1024];

	public:
		sem_t mtx_direction;
		Paddle(int velocity, const int up_press, const int up_release, const int down_press, const int down_release);
		int lives();
		void move(int direction);
		void run();
};

#endif
