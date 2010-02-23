#include <l4/kbd/kbd.h>

#include <l4/util/port_io.h>

#include <l4/cxx/iostream.h>
#include <l4/io/io.h>
#include <l4/re/env>
#include <l4/re/namespace>
#include <l4/re/protocols>
#include <l4/re/service-sys.h>
#include <l4/re/util/cap_alloc>
#include <l4/re/util/object_registry>
#include <l4/sys/capability>
#include <l4/sys/irq>
#include <l4/sys/kdebug.h>
#include <l4/util/atomic.h>
#include <l4/util/port_io.h>
#include <pthread.h>
#include <pthread-l4.h>

#include <iostream>
#include <stdlib.h>

static L4Re::Util::Object_registry registry(
	L4Re::Env::env()->main_thread(),
	L4Re::Env::env()->factory()
);

static L4::Server<L4::Basic_registry_dispatcher> server(l4_utcb());

static Kbd_server *driver;

namespace KBD
{
	void lock(l4_umword_t *mutex)
	{
		while(!l4util_cmpxchg(mutex, 0, 1));
	}

	void unlock(l4_umword_t *mutex)
	{
		*mutex = 0;
	}
};

l4_umword_t mutex = 0;

/*
Process any keyboard interrupts, notifying the Kbd_server with the
scancode read in. Run in separate thread.
*/
static void *poll(void*)
{
	l4_umword_t label;
	int scancode = 0;

	printf("poll(): Starting to poll ...\n");

	while(1) {
		printf("poll(): Waiting on IRQ.\n");
		
		irqcap->wait(&label);
		//enter_kdebug("poll(): Waking on IRQ.");
		printf("poll(): Interrupt received.\n");
		
		l4io_request_ioport(0x60, 1);
		scancode = l4util_in8(0x60);
		printf("poll(): Received scancode [%i].\n", scancode);
		
		driver->push(scancode);
	}
	return NULL;
}

Kbd_server::Kbd_server()
{
	queues = new std::list<EventQueue>();
}

void Kbd_server::push(int scancode)
{
	printf("Pushing scancode %i to event queues.\n", scancode);
	std::list<EventQueue>::iterator iter;
	for (iter = queues->begin(); iter != queues->end(); iter++) {
		iter->push(scancode);
	}
}

int Kbd_server::dispatch(l4_umword_t obj, L4::Ipc_iostream &ios)
{
	//enter_kdebug("kdb->open()");
	printf("Kbd_server: dispatching.\n");
	
	l4_msgtag_t tag;
	ios >> tag;
	if (tag.label() != L4Re::Protocol::Service) {
		return -L4_EBADPROTO;
	}
	L4::Opcode opcode;
	ios >> opcode;
	switch (opcode) {
		case L4Re::Service_::Open:
			printf("Kbd_server: creating service.\n");
			{
				EventQueue *evq = new EventQueue();
				printf("Kbd_server: created EventQueue.\n");
				KBD::lock(&mutex);
				queues->push_back(*evq);
				KBD::unlock(&mutex);
				registry.register_obj(evq);
				ios << evq->obj_cap();
				printf("Kbd_server: handing out EventQueue %p.\n", evq);
			}
			return L4_EOK;
		default:
			return -L4_ENOSYS;
	}
	return 0;
}

EventQueue::EventQueue()
{
	mutex = 0;
	scancodes = new std::list<int>();

	// Create a new Thread for use with this EventQueue.
	L4::Cap<L4::Thread> thread = L4Re::Util::cap_alloc.alloc<L4::Thread>();
	if (!thread.is_valid()) {
		printf("EventQueue: could not get a valid Thread capability!\n");
	}
	if (l4_error(L4Re::Env::env()->factory()->create_thread(L4Re::Env::env()->main_thread()))) {
		printf("EventQueue: could not create Thread!\n");
	}
	printf("EventQueue: got thread cap.\n");

	// FIXME: I don't understand this ...
	if (l4_error(L4Re::Env::env()->factory()->create_gate(
		L4Re::Env::env()->main_thread(),
		thread,
		0xD00F
	))) {
		printf("Could not create IPC gate!\n");
	}

	// Create an IRQ for this EventQueue to wait on if no events are pending.
	fresh = L4Re::Util::cap_alloc.alloc<L4::Irq>();

	if (!fresh.is_valid()) {
		printf("Could not get a valid IRQ capability for the EventQueue!\n");
		exit(1);
	}

	if (l4_error(L4Re::Env::env()->factory()->create_irq(fresh))) {
		printf("Could not create IRQ for the EventQueue!\n");
		exit(1);
	}

	printf("Attaching to IRQ.\n");
	if (l4_error(fresh->attach(0xBEEF, 0, thread))) {
		printf("Could not attach to IRQ in EventQueue!\n");
		exit(1);
	}

}

void EventQueue::push(int scancode)
{
	printf("EventQueue @%p: Received scancode %i.\n", this, scancode);
	KBD::lock(&mutex);
	scancodes->push_back(scancode);
	// Wake any previously blocked client.
	KBD::unlock(&mutex);
	printf("EventQueue @%p: Waking clients.\n", this);
	fresh->trigger();
}

int EventQueue::dispatch(l4_umword_t obj, L4::Ipc_iostream &ios)
{
	//enter_kdebug("EventQueue::dispatch");
	printf("EventQueue %p: Dispatching.\n", this);
	l4_msgtag_t tag;
	ios >> tag;
	if (tag.label() != 0) {
		return -L4_EBADPROTO;
	}
	if (scancodes->empty()) {
		l4_umword_t label;
		printf("EventQueue %p: No scancodes queued, blocking.\n", this);
		//enter_kdebug("Sleep on empty EventQueue.");
		fresh->wait(&label);
		//enter_kdebug("Waking up on EventQueue refill.");
		printf("EventQueue %p: Processing retriggered.\n", this);
	}
	KBD::lock(&mutex);
	int scancode = scancodes->front();
	printf("Got first scancode in queue: %i\n", scancode);
	scancodes->pop_front();
	KBD::unlock(&mutex);
	ios << scancode;
	return L4_EOK;
}

int main(int argc, char **argv)
{
	printf("Starting keyboard driver.\n");

	pthread_t poller;

	/* Get an IRQ capability for the polling thread. */
	irqcap = L4Re::Util::cap_alloc.alloc<L4::Irq>();
	if (!irqcap.is_valid()) {
		printf("Could not get a valid IRQ capability slot!\n");
		exit(1);
	}
	printf("Got IRQ cap.\n");

	/* Create IRQ object for the polling thread. */
	if (l4_error(L4Re::Env::env()->factory()->create_irq(irqcap))) {
		printf("Could not create IRQ!\n");
		exit(1);
	}
	printf("Created IRQ.\n");
	
	/* Create the polling thread. */
	if (int err = pthread_create(&poller, NULL, &poll, NULL)) {
		printf("Failed to create polling thread! [%i]!\n", err);
	}
	printf("Created polling thread.\n");
	
	/* Attach the polling thread to its IRQ object. */
	if (l4_error(irqcap->attach(0xBEEF, 0, L4::Cap<L4::Thread>(pthread_getl4cap(poller))))) {
		printf("Error while attaching polling thread to IRQ!\n");
		exit(1);
	}
	printf("Attached polling thread to IRQ.\n");

	/* Get an ICU capability. */
	icucap = L4Re::Util::cap_alloc.alloc<L4::Icu>();
	if (!icucap.is_valid()) {
		printf("Could not get a valid ICU capability slot!\n");
		exit(1);
	}
	printf("Got ICU cap.\n");

	/* Associate the hardware ICU with this capability. */
	if (L4Re::Env::env()->names()->query("icu", icucap)) {
		printf("Could not get an ICU capability!\n");
		exit(1);
	}
	printf("Bound to hardware ICU.\n");

	/* Bind IRQ 0x1 to ICU. */
	if (l4_error(icucap->bind(0x1, irqcap))) {
		printf("Error while binding IRQ 0x1 to ICU!\n");
		exit(1);
	}
	printf("Bound to interrupt 0x1 in ICU.\n");

	driver = new Kbd_server();
	

	/*pthread_t poller;
	printf("Creating polling thread ... ");
	if (int err = pthread_create(&poller, NULL, &poll, NULL)) {
		printf("Failed to create polling thread! [" << err << "]");
	}*/

	// Publish the service.
	registry.register_obj(driver);
	if (L4Re::Env::env()->names()->register_obj("driver", driver->obj_cap())) {
		printf("Could not register service, probably read-only namespace?\n");
		return 1;
	}
	printf("Registered driver.\n");

	// Start dispatching.
	printf("Keyboard driver started.\n");
	server.loop();

	return 0;
}
