#include <l4/console/console.h>
#include <l4/fb_muxer/fb_muxer.h>

#include <l4/cxx/ipc_stream>
#include <l4/re/env>
#include <l4/re/util/cap_alloc>

#include <stdio.h>
#include <stdlib.h>

int main(int argc, char *args) {
	L4::Cap<void> kbd = L4Re::Util::cap_alloc.alloc<void>();
	if (!kbd.is_valid()) {
		printf("ERROR: Could not get a valid capability!\n");
		
		exit(1);
	}
	if (L4Re::Env::env()->names()->query("kbd", kbd)) {
		printf("ERROR: Could not query 'kbd' capability!\n");

		exit(1);
	}
	L4::Cap<void> console = L4Re::Util::cap_alloc.alloc<void>();
	if (!console.is_valid()) {
		printf("ERROR: Could not get a valid capability!\n");

		exit(1);
	}
	if (L4Re::Env::env()->names()->query("console", console)) {
		printf("ERROR: Could not query 'console' capability!\n");

		exit(1);
	}
	L4::Cap<void> muxer = L4Re::Util::cap_alloc.alloc<void>();
	if (!muxer.is_valid()) {
		printf("ERROR: Could not get a valid capability!\n");

		exit(1);
	}
	if (L4Re::Env::env()->names()->query("fb_muxer", muxer)) {
		printf("ERROR: Could not query 'fb_muxer' capability!\n");

		exit(1);
	}

	L4::Ipc_iostream ios(l4_utcb());
	l4_msgtag_t result;

	int scancode = -1;

	while (1) {

		ios.reset();
		result = ios.call(kbd.cap(), 0);
		
		if (l4_error(result)) {
			printf("ERROR: Could not read scancode! [%i]\n", result);
		}
		
		ios >> scancode;

		switch (scancode) {
			case 59:
				printf("Switching to viewport 0.\n");
				ios.reset();
				ios << L4::Opcode(Switch) << 0;
				result = ios.call(muxer.cap(), Switch);
				break;
			case 60:
				printf("Switching to viewport 1.\n");
				ios.reset();
				ios << L4::Opcode(Switch) << 1;
				result = ios.call(muxer.cap(), Switch);
				break;
			case 71:
				ios.reset();
				ios << L4::Opcode(Opcode::Scroll) << TOP;
				result = ios.call(console.cap(), Protocol::Console);
				break;
			case 72:
				ios.reset();
				ios << L4::Opcode(Opcode::Scroll) << LINE_UP;
				result = ios.call(console.cap(), Protocol::Console);
				break;
			case 73:
				ios.reset();
				ios << L4::Opcode(Opcode::Scroll) << PAGE_UP;
				result = ios.call(console.cap(), Protocol::Console);
				break;
			case 79:
				ios.reset();
				ios << L4::Opcode(Opcode::Scroll) << BOTTOM;
				result = ios.call(console.cap(), Protocol::Console);
				break;
			case 80:
				ios.reset();
				ios << L4::Opcode(Opcode::Scroll) << LINE_DOWN;
				result = ios.call(console.cap(), Protocol::Console);
				break;
			case 81:
				ios.reset();
				ios << L4::Opcode(Opcode::Scroll) << PAGE_DOWN;
				result = ios.call(console.cap(), Protocol::Console);
				break;
			default:
				break;
		}

		if (l4_error(result)) {
			printf("ERROR: Processing the request failed! [%i]\n", result);
		}

	}
	
	return 0;
}
