#ifndef PADDLE_H
#define PADDLE_H

#include <l4/cxx/thread.h>

class Paddle : cxx::Thread
{
	private:
		char up_press, up_release;
		char down_press, down_release;
		int position;
		int velocity;
		L4::Cap<void> kbd;
		L4::Cap<void> svr;

		char stack[1024];
	public:
		Paddle(int velocity, char up_press, char up_release, char down_press, char down_release);
		void run();
};

#endif
