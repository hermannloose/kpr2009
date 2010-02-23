#include <l4/console/console.h>

#include <l4/cxx/ipc_stream>
#include <l4/re/env>
#include <l4/re/util/cap_alloc>
#include <l4/sys/kdebug.h>

#include <iostream>

#include <stdlib.h>

L4::Cap<void> console;
L4::Cap<void> kbd;

int main(int argc, char* argv[])
{
  printf("Starting client.\n");
	
	L4::Ipc_iostream console_str(l4_utcb());
	L4::Ipc_iostream kbd_str(l4_utcb());

	console = L4Re::Util::cap_alloc.alloc<void>();
	if (!console.is_valid()) {
		printf("Could not get valid capability slot!\n");
		return 1;
	}

	if (L4Re::Env::env()->names()->query("console", console)) {
		printf("Could not get 'console' capability!\n");
		return 1;
	}
	printf("Got console cap.\n");

	console_str << l4_umword_t(Opcode::Put) << "Client connected.";
	l4_msgtag_t result = console_str.call(console.cap(), Protocol::Console);
	if (l4_ipc_error(result, l4_utcb())) {
		printf("Could not call console with initial message!\n");
	}
	printf("Printed hello.\n");

	L4::Cap<void> kbd = L4Re::Util::cap_alloc.alloc<void>();
	if (!kbd.is_valid()) {
		printf("Could not get valid capability slot!\n");
		return 1;
	}

	if (L4Re::Env::env()->names()->query("kbd", kbd)) {
		printf("Could not get 'kbd' capability!\n");
		return 1;
	}
	printf("Got kbd cap.\n");

  printf("Console client started.\n");

	int scancode;

	while (1) {
		kbd_str.reset();
		kbd_str << l4_umword_t(0);
		//enter_kdebug("Calling kbd for scancode.");
		printf("Calling kbd for scancode.\n");
		result = kbd_str.call(kbd.cap(), 0);
		if (l4_ipc_error(result, l4_utcb())) {
			printf("Error reading scancode from kbd!\n");
		}
		kbd_str >> scancode;
		printf("Received [%i].\n", scancode);
		/*char msg[2];
		msg[0] = scancode;
		msg[1] = '\0';
		s << Opcode::Put << msg;*/
		console_str.reset();
		console_str << l4_umword_t(Opcode::Put) << "Foo.";
		result = console_str.call(console.cap(), Protocol::Console);
		if (l4_ipc_error(result, l4_utcb())) {
			printf("Error writing scancode to console!\n");
		}
		printf("Called console.\n");

	}
	
	return 0;
}
