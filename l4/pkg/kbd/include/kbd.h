#ifndef KBD_SERVER
#define KBD_SERVER

#include <l4/cxx/iostream.h>
#include <l4/sys/irq>
#include <l4/sys/types.h>

class Kbd_server
{
  private:
	  L4::Cap<L4::Icu> icucap;
	  L4::Cap<L4::Irq> irqcap;
  public:
	  Kbd_server();
		void process();
};

#endif
