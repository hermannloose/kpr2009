#include <l4/console/console.h>
#include <l4/fb_muxer/fb_muxer.h>

#include <l4/cxx/ipc_stream>
#include <l4/re/env>
#include <l4/re/util/cap_alloc>
#include <l4/sys/types.h>

#include <stdio.h>
#include <unistd.h>

int main(int argc, char* argv[])
{
	L4::Cap<void> fb_muxer = L4Re::Util::cap_alloc.alloc<void>();
	L4Re::Env::env()->names()->query("fb_muxer", fb_muxer);
	L4::Cap<void> console1 = L4Re::Util::cap_alloc.alloc<void>();
	L4Re::Env::env()->names()->query("console1", console1);
	L4::Cap<void> console2 = L4Re::Util::cap_alloc.alloc<void>();
	L4Re::Env::env()->names()->query("console2", console2);
	L4::Ipc_iostream ios(l4_utcb());
	while (1) {
		printf("Console 0\n");
		ios.reset();
		ios << l4_umword_t(Switch) << 0;
		ios.call(fb_muxer.cap(), Switch);
		ios.reset();
		ios << l4_umword_t(Opcode::Refresh);
		ios.call(console1.cap(), Protocol::Console); 
		sleep(5);
		printf("Console 1\n");
		ios.reset();
		ios << l4_umword_t(Switch) << 1;
		ios.call(fb_muxer.cap(), Switch);
		ios.reset();
		ios << l4_umword_t(Opcode::Refresh);
		ios.call(console2.cap(), Protocol::Console);
		sleep(5);
	}
}
