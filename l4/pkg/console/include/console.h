#ifndef CONSOLE_H
#define CONSOLE_H

#include <l4/cxx/iostream.h>
#include <l4/cxx/ipc_server>
#include <l4/re/fb>
#include <l4/re/util/cap_alloc>
#include <l4/sys/types.h>

class Console_server : public L4::Server_object
{
  private:
    L4::Cap<L4Re::Framebuffer> fb;
    l4_addr_t base_addr;
    
  public:
    Console_server();
    int dispatch(l4_umword_t obj, L4::Ipc_iostream &ios);
};

#endif
