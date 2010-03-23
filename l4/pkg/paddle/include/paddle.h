#ifndef PADDLE_H
#define PADDLE_H

#include <l4/cxx/thread.h>

#include <semaphore.h>

class Paddle;

class Input : public cxx::Thread
{
	private:
		Paddle *parent;
		int up_press, up_release;
		int down_press, down_release;
		L4::Cap<void> kbd;
		char stack[1024];
	public:
		Input(Paddle *parent, int up_press, int up_release, int down_press, int down_release);
		void run();
};

class Paddle : public cxx::Thread
{
	private:
		sem_t mutex;
		int up_press, up_release;
		int down_press, down_release;
		int position;
		int velocity;
		int direction;
		L4::Cap<void> kbd;
		L4::Cap<void> svr;
		unsigned long paddle;

		char stack[1024];
	public:
		Paddle(int velocity, const int up_press, const int up_release, const int down_press, const int down_release);
		int lives();
		void move(int direction);
		void run();
};

#endif
