#include <l4/kbd/kbd.h>

#include <l4/cxx/iostream.h>
#include <l4/re/env>
#include <l4/re/util/cap_alloc>
#include <l4/sys/irq>
#include <l4/sys/types.h>

#include <iostream>
#include <stdlib.h>

Kbd_server::Kbd_server()
{
  icucap = L4Re::Util::cap_alloc.alloc<L4::Icu>();
	if (!icucap.is_valid()) {
	  std::cerr << "Could not get a valid ICU capability slot!" << std::endl;
		exit(1);
	}
	if (L4Re::Env::env()->names()->query("icu", icucap)) {
	  std::cerr << "Could not get an ICU capability!" << std::endl;
		exit(1);
	}
}

int Kbd_server::dispatch(l4_umword_t obj, L4::Ipc_iostream &ios)
{
}

int main(int argc, char **argv)
{
}
