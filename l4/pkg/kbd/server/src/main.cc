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

static void *poll(void*)
{
	l4_umword_t label;
	int scancode = 0;

	std::cout << "poll(): Starting to poll ..." << std::endl;

	while(1) {
		std::cout << "poll(): Waiting on IRQ." << std::endl;
		
		irqcap->wait(&label);
		//enter_kdebug("poll(): Waking on IRQ.");
		std::cout << "poll(): Interrupt received." << std::endl;
		
		l4io_request_ioport(0x60, 1);
		scancode = l4util_in8(0x60);
		std::cout << "poll(): Received scancode [" << scancode << "]." << std::endl;
		
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
	std::cout << "Pushing scancode " << scancode << " to event queues." << std::endl;
	std::list<EventQueue>::iterator iter;
	for (iter = queues->begin(); iter != queues->end(); iter++) {
		iter->push(scancode);
	}
}

int Kbd_server::dispatch(l4_umword_t obj, L4::Ipc_iostream &ios)
{
	//enter_kdebug("kdb->open()");
	std::cout << "Keyboard driver called." << std::endl;
	
	l4_msgtag_t tag;
	ios >> tag;
	if (tag.label() != L4Re::Protocol::Service) {
		return -L4_EBADPROTO;
	}
	L4::Opcode opcode;
	ios >> opcode;
	switch (opcode) {
		case L4Re::Service_::Open:
			std::cout << "Opening event service ..." << std::endl;
			{
				EventQueue *evq = new EventQueue();
				queues->push_back(*evq);
				registry.register_obj(evq);
				ios << evq->obj_cap();
				//printf("Handed out EventQueue %p.\n", evq);
			}
			return L4_EOK;
		default:
			return -L4_ENOSYS;
	}
	return 0;
}

EventQueue::EventQueue()
{
	scancodes = new std::list<int>();
	
	fresh = L4Re::Util::cap_alloc.alloc<L4::Irq>();
	
	if (!fresh.is_valid()) {
		std::cerr << "Could not get a valid IRQ capability for the EventQueue!" << std::endl;
		exit(1);
	}

	if (l4_error(L4Re::Env::env()->factory()->create_irq(fresh))) {
		std::cerr << "Could not create IRQ for the EventQueue!" << std::endl;
		exit(1);
	}

	if (l4_error(fresh->attach(0xBEEF, 0, L4Re::Env::env()->main_thread()))) {
		std::cerr << "Could not attach to IRQ in EventQueue!" << std::endl;
		exit(1);
	}

}

void EventQueue::push(int scancode)
{
	printf("EventQueue @%p: Received scancode %i.\n", this, scancode);
	scancodes->push_back(scancode);
	// Wake any previously blocked client.
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
		fresh->wait(&label);
		printf("EventQueue %p: Processing retriggered.\n", this);
	}
	int scancode = scancodes->front();
	scancodes->pop_front();
	ios << scancode;
	return L4_EOK;
}

int main(int argc, char **argv)
{
	std::cout << "Starting keyboard driver ..." << std::endl;

	pthread_t poller;

	/* Get an IRQ capability. */
	irqcap = L4Re::Util::cap_alloc.alloc<L4::Irq>();
	if (!irqcap.is_valid()) {
		std::cerr << "Could not get a valid IRQ capability slot!" << std::endl;
		exit(1);
	}
	/* Create IRQ. */
	if (l4_error(L4Re::Env::env()->factory()->create_irq(irqcap))) {
		std::cerr << "Could not create IRQ!" << std::endl;
		exit(1);
	}
	
	if (int err = pthread_create(&poller, NULL, &poll, NULL)) {
		std::cerr << "Failed to create polling thread! [" << err << "]" << std::endl;
	}
	
	/* Attach to IRQ. */
	//if (l4_error(irqcap->attach(0xBEEF, 0, L4Re::Env::env()->main_thread()))) {
	if (l4_error(irqcap->attach(0xBEEF, 0, L4::Cap<L4::Thread>(pthread_getl4cap(poller))))) {
		std::cerr << "Error while attaching worker thread to IRQ!" << std::endl;
		exit(1);
	}

	/* Get an ICU capability. */
	icucap = L4Re::Util::cap_alloc.alloc<L4::Icu>();
	if (!icucap.is_valid()) {
		std::cerr << "Could not get a valid ICU capability slot!" << std::endl;
		exit(1);
	}
	/* Associate the hardware ICU with this capability. */
	if (L4Re::Env::env()->names()->query("icu", icucap)) {
		std::cerr << "Could not get an ICU capability!" << std::endl;
		exit(1);
	}
	/* Bind IRQ 0x1 to ICU. */
	if (l4_error(icucap->bind(0x1, irqcap))) {
		std::cerr << "Error while binding IRQ 0x1 to ICU!\n" << std::endl;
		exit(1);
	}

	driver = new Kbd_server();
	

	/*pthread_t poller;
	std::cout << "Creating polling thread ... " << std::endl;
	if (int err = pthread_create(&poller, NULL, &poll, NULL)) {
		std::cerr << "Failed to create polling thread! [" << err << "]" << std::endl;
	}*/

	// Publish the service.
	registry.register_obj(driver);
	if (L4Re::Env::env()->names()->register_obj("driver", driver->obj_cap())) {
		std::cerr << "Could not register service, probably read-only namespace?" << std::endl;
		return 1;
	}
	// Start dispatching.
	std::cout << "Keyboard driver started. Dispatching ..." << std::endl;
	server.loop();

	return 0;
}
