#ifndef KBD_SERVER
#define KBD_SERVER

#include <l4/cxx/ipc_server>
#include <l4/cxx/iostream.h>
#include <l4/sys/irq>
#include <l4/sys/types.h>

#include <list>
#include <pthread.h>
#include <semaphore.h>

static L4::Cap<L4::Icu> icucap;
static L4::Cap<L4::Irq> irqcap;

static void *poll(void*);

class Kbd_server;
class EventQueue;

class Kbd_server : public L4::Server_object
{
	private:
		std::list<EventQueue*> *queues;
		sem_t list_access;
  public:
	  Kbd_server();
		void push(int scancode);
		int dispatch(l4_umword_t obj, L4::Ipc_iostream &ios);
};

class EventQueue : public L4::Server_object
{
	private:
		std::list<int> *scancodes;
		sem_t list_access;
		sem_t empty;
	public:
		static void *create(void *args);
		EventQueue();
		void push(int scancode);
		int dispatch(l4_umword_t obj, L4::Ipc_iostream &ios);
};

#endif
