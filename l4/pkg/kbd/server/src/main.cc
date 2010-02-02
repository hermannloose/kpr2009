#include <l4/kbd/kbd.h>

#include <l4/cxx/iostream.h>
#include <l4/re/env>
#include <l4/re/util/cap_alloc>
#include <l4/re/util/object_registry>
#include <l4/sys/irq>
#include <l4/sys/types.h>

#include <iostream>
#include <stdlib.h>

static L4Re::Util::Object_registry registry(
	L4Re::Env::env()->main_thread(),
	L4Re::Env::env()->factory()
);

Kbd_server::Kbd_server()
{
  l4_msgtag_t tag;

	// Create and fill ICU cap.
	icucap = L4Re::Util::cap_alloc.alloc<L4::Icu>();
	if (!icucap.is_valid())
	{
		std::cerr << "Could not get a valid ICU capability slot!" << std::endl;
		exit(1);
	}
	if (L4Re::Env::env()->names()->query("icu", icucap))
	{
		std::cerr << "Could not get an ICU capability!" << std::endl;
		exit(1);
	}

	// Create and fill IRQ cap.
	irqcap = L4Re::Util::cap_alloc.alloc<L4::Irq>();
	if (!irqcap.is_valid())
	{
		std::cerr << "Could not get a valid IRQ capability slot!" << std::endl;
		exit(1);
	}
	tag = L4Re::Env::env()->factory()->create_irq(irqcap);
	if (l4_error(tag))
	{
		std::cerr << "Could not create IRQ object!" << std::endl;
		exit(1);
	}

  // Bind IRQ to ICU.
	tag = icucap->bind(0x1, irqcap);
	if (l4_error(tag))
	{
		std::cerr << "Binding IRQ cap to IRQ #1 failed!" << std::endl;
		exit(1);
	}

	// Attach to IRQ.
	tag = irqcap->attach(0xBEEF, 0, L4Re::Env::env()->main_thread());
	if (l4_error(tag))
	{
	  std::cerr << "Could not attach to IRQ!" << std::endl;
		exit(1);
	}

	// Publish IRQ.
	registry.register_obj(irqcap);
	L4Re::Env::env()->names()->register_obj("irq", irqcap);
}

void Kbd_server::process()
{
	while (true)
	{
  	irqcap->receive(L4_IPC_NEVER); 
	}
}

int main(int argc, char **argv)
{
	Kbd_server *srv = new Kbd_server();
	srv->process();
}
