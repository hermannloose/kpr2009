#ifndef KBD_SERVER
#define KBD_SERVER

#include <l4/cxx/ipc_server>
#include <l4/cxx/iostream.h>
#include <l4/sys/irq>
#include <l4/sys/types.h>

class Kbd_server : public L4::Server_object
{
  private:
	  L4::Cap<L4::Icu> icucap;
	  L4::Cap<L4::Irq> irqcap;
  public:
	  Kbd_server();
		int dispatch(l4_umword_t obj, L4::Ipc_iostream &ios);
};

#endif
