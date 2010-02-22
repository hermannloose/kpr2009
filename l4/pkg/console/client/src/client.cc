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
  std::cout << "Starting console client ..." << std::endl;
	
	L4::Ipc_iostream console_str(l4_utcb());
	L4::Ipc_iostream kbd_str(l4_utcb());

	console = L4Re::Util::cap_alloc.alloc<void>();
	if (!console.is_valid()) {
		std::cerr << "Could not get valid capability slot!" << std::endl;
		return 1;
	}

	if (L4Re::Env::env()->names()->query("console", console)) {
		std::cerr << "Could not get 'console' capability!" << std::endl;
		return 1;
	}

	console_str << l4_umword_t(Opcode::Put) << "Client connected.";
	l4_msgtag_t result = console_str.call(console.cap(), Protocol::Console);
	if (l4_ipc_error(result, l4_utcb())) {
		std::cerr << "Could not call console with initial message!" << std::endl;
	}

	L4::Cap<void> kbd = L4Re::Util::cap_alloc.alloc<void>();
	if (!kbd.is_valid()) {
		std::cerr << "Could not get valid capability slot!" << std::endl;
		return 1;
	}

	if (L4Re::Env::env()->names()->query("kbd", kbd)) {
		std::cerr << "Could not get 'kbd' capability!" << std::endl;
		return 1;
	}

  std::cout << "Console client started." << std::endl;

	int scancode;

	while (1) {
		kbd_str.reset();
		kbd_str << l4_umword_t(0);
		//enter_kdebug("Calling kbd for scancode.");
		std::cout << "Calling kbd for scancode." << std::endl;
		result = kbd_str.call(kbd.cap(), 0);
		if (l4_ipc_error(result, l4_utcb())) {
			std::cerr << "Error reading scancode from kbd!" << std::endl;
		}
		kbd_str >> scancode;
		std::cout << "Received [" << scancode << "]." << std::endl;
		/*char msg[2];
		msg[0] = scancode;
		msg[1] = '\0';
		s << Opcode::Put << msg;*/
		console_str.reset();
		console_str << l4_umword_t(Opcode::Put) << "Foo.";
		result = console_str.call(console.cap(), Protocol::Console);
		if (l4_ipc_error(result, l4_utcb())) {
			std::cerr << "Error writing scancode to console!" << std::endl;
		}
		std::cout << "Called console." << std::endl;

	}
	
	return 0;
}
