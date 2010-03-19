#ifndef PADDLE_H
#define PADDLE_H

#include <l4/cxx/thread.h>

class Paddle : cxx::Thread
{
	private:
		int up_press, up_release;
		int down_press, down_release;
		int position;
		int velocity;
		L4::Cap<void> kbd;
		L4::Cap<void> svr;
		unsigned long paddle;

		char stack[1024];
	public:
		Paddle(int velocity, int up_press, int up_release, int down_press, int down_release);
		int lives();
		void move(int pos);
		void run();
};

#endif
