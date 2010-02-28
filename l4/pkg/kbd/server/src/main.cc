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
#include <l4/sys/semaphore>
#include <l4/sys/__timeout.h>
#include <l4/util/port_io.h>
#include <pthread.h>
#include <pthread-l4.h>
#include <semaphore.h>

#include <iostream>
#include <stdlib.h>

// The object registry for use with the main Kbd_server.
static L4Re::Util::Object_registry registry(
	L4Re::Env::env()->main_thread(),
	L4Re::Env::env()->factory()
);

static L4::Server<L4::Basic_registry_dispatcher> server(l4_utcb());

static Kbd_server *driver;

//static L4::Semaphore *sem;

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

static void *queue_worker(void *args)
{
	L4::Server<L4::Basic_registry_dispatcher> worker(l4_utcb());
	printf("Thread %x starting server loop ...\n", pthread_self());
	worker.loop();
	return NULL;
}

Kbd_server::Kbd_server()
{
	queues = new std::list<EventQueue>();
	sem_init(&list_access, 0, 1);
}

void Kbd_server::push(int scancode)
{
	printf("Kbd_server: push(): %i\n", scancode);
	std::list<EventQueue>::iterator iter;
	for (iter = queues->begin(); iter != queues->end(); iter++) {
		iter->push(scancode);
	}
}

int Kbd_server::dispatch(l4_umword_t obj, L4::Ipc_iostream &ios)
{
	printf("Thread: %x\n", pthread_self());
	//enter_kdebug("kdb->open()");
	printf("Kbd_server: dispatching.\n");
	
	l4_msgtag_t tag;
	ios >> tag;
	if (tag.label() != L4Re::Protocol::Service) {
		printf("Kbd_server: unsupported protocol!\n");
		return -L4_EBADPROTO;
	}
	L4::Opcode opcode;
	ios >> opcode;
	switch (opcode) {
		case L4Re::Service_::Open:
			printf("Kbd_server: creating service.\n");
			{
				// Create a new worker thread for dispatching in EventQueue.
				pthread_t worker;
				pthread_create(&worker, NULL, &queue_worker, NULL);
				printf("Worker thread: %x\n", worker);

				L4Re::Util::Object_registry *reg = new L4Re::Util::Object_registry(
					L4::Cap<L4::Thread>(pthread_getl4cap(worker)),
					L4Re::Env::env()->factory()
				);

				EventQueue *evq = new EventQueue();
				printf("Kbd_server: created EventQueue.\n");
				
				sem_wait(&list_access);
				queues->push_back(*evq);
				sem_post(&list_access);
			
				reg->register_obj(evq);
				printf("Kbd_server: registered EventQueue.\n");

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
	l4_msgtag_t err;

	scancodes = new std::list<int>();

	// Create semaphore.
	/*L4::Cap<L4::K_semaphore> ksem;
	if (l4_error(err = L4Re::Env::env()->factory()->create_semaphore(ksem))) {
		printf("EventQueue: could not create kernel semaphore! [%i]\n", err);
	}
	empty = new L4::Semaphore(ksem);
	empty->init(0);*/
	sem_init(&list_access, 0, 1);
	sem_init(&empty, 0, 0);
	printf("EventQueue: created semaphore.\n");
}

void EventQueue::push(int scancode)
{
	printf("Thread: %x\n", pthread_self());
	printf("EventQueue: received scancode %i.\n", scancode);
	sem_wait(&list_access);
	printf("DOWN on list_access\n");
	scancodes->push_back(scancode);
	sem_post(&list_access);
	printf("UP on list_access\n");
	enter_kdebug("EventQueue: pre-up on empty in push()");
	sem_post(&empty);
	enter_kdebug("EventQueue: up on empty in push()");
	printf("UP on empty\n");
}

int EventQueue::dispatch(l4_umword_t obj, L4::Ipc_iostream &ios)
{
	printf("[%x] EventQueue: dispatching.\n", pthread_self());
	//enter_kdebug("EventQueue::dispatch");
	l4_msgtag_t tag;
	ios >> tag;
	if (tag.label() != 0) {
		printf("[%x] EventQueue: unsupported protocol!\n", pthread_self());
		return -L4_EBADPROTO;
	}
	printf("[%x] EventQueue: checking for scancodes ...\n", pthread_self());
	enter_kdebug("EventQueue: before testing 'empty'");
	sem_wait(&empty);
	printf("DOWN on empty\n");
	sem_wait(&list_access);
	printf("DOWN on list_access\n");
	int scancode = scancodes->front();
	printf("EventQueue: got first scancode in queue: %i\n", scancode);
	scancodes->pop_front();
	sem_post(&list_access);
	printf("UP on list_access\n");
	ios << scancode;
	return L4_EOK;
}

int main(int argc, char **argv)
{
	l4_msgtag_t err;

	printf("Starting keyboard driver.\n");

	/*L4::Cap<L4::K_semaphore> ksem;
	if (l4_error(L4Re::Env::env()->factory()->create_semaphore(ksem))) {
		printf("Could not create kernel semaphore!\n");
		exit(1);
	}
	sem = new L4::Semaphore(ksem);
	sem->init(1);
	printf("Initialized semaphore.\n");*/

	pthread_t poller;

	/* Get an IRQ capability for the polling thread. */
	irqcap = L4Re::Util::cap_alloc.alloc<L4::Irq>();
	if (!irqcap.is_valid()) {
		printf("Could not get a valid IRQ cap!\n");
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
		printf("Could not create polling thread! [%i]!\n", err);
	}
	printf("Created polling thread.\n");
	
	/* Attach the polling thread to its IRQ object. */
	if (l4_error(err = irqcap->attach(0xbeef0001, 0, L4::Cap<L4::Thread>(pthread_getl4cap(poller))))) {
		printf("Error while attaching polling thread to IRQ! [%i]\n", err);
		exit(1);
	}
	printf("Attached polling thread to IRQ.\n");

	/* Get an ICU capability. */
	icucap = L4Re::Util::cap_alloc.alloc<L4::Icu>();
	if (!icucap.is_valid()) {
		printf("Could not get a valid ICU cap!\n");
		exit(1);
	}
	printf("Got ICU cap.\n");

	/* Associate the hardware ICU with this capability. */
	if (L4Re::Env::env()->names()->query("icu", icucap)) {
		printf("Could not get hardware ICU!\n");
		exit(1);
	}
	printf("Got hardware ICU.\n");

	/* Bind IRQ 0x1 to ICU. */
	if (l4_error(err = icucap->bind(0x1, irqcap))) {
		printf("Could not bind IRQ 0x1 to ICU! [%i]\n", err);
		exit(1);
	}
	printf("Bound to interrupt 0x1 in ICU.\n");

	driver = new Kbd_server();

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
