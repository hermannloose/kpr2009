#include <l4/console/console.h>

#include <l4/cxx/ipc_stream>
#include <l4/re/env>
#include <l4/re/util/cap_alloc>

#include <iostream>

L4::Cap<void> console;
L4::Cap<void> kbd;

int main(int argc, char* argv[])
{
  std::cout << "Starting console client ..." << std::endl;
	
	console = L4Re::Util::cap_alloc.alloc<void>();
	if (!console.is_valid()) {
		std::cerr << "Could not get valid capability slot!" << std::endl;
		return 1;
	}

	if (L4Re::Env::env()->names()->query("console", console)) {
		std::cerr << "Could not get 'console' capability!" << std::endl;
		return 1;
	}

	L4::Ipc_iostream s(l4_utcb());
	s << l4_umword_t(Opcode::Put) << "Client connected.";
	l4_msgtag_t result = s.call(console.cap(), Protocol::Console);
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
  
	while(1);
	
	return 0;
}
