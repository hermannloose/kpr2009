#ifndef PADDLE_H
#define PADDLE_H

#include <l4/sys/capability>

#include <semaphore.h>

enum {
	UP_PRESS, UP_RELEASE, DOWN_PRESS, DOWN_RELEASE
};

enum {
	NEUTRAL, UP, DOWN
};

class Paddle
{
	private:
		int up_press, up_release;
		int down_press, down_release;
		int pressed;
		sem_t mtx_pressed;
		int position;
		int velocity;
		int direction;
		sem_t mtx_direction;
		L4::Cap<void> kbd;
		L4::Cap<void> svr;
		L4::Cap<void> console;
		unsigned long paddle;	
	public:
		Paddle(int velocity, const int up_press, const int up_release, const int down_press, const int down_release);
		int lives();
		void move(int direction);
		void press(int scancode);
		void run();
};

#endif
