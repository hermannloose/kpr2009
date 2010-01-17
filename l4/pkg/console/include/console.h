#ifndef CONSOLE_H
#define CONSOLE_H

#include <l4/cxx/iostream.h>
#include <l4/cxx/ipc_server>
#include <l4/re/fb>
#include <l4/re/util/cap_alloc>
#include <l4/sys/types.h>

#include <list>
#include <string>

class Console_server : public L4::Server_object
{
  private:
    L4::Cap<L4Re::Framebuffer> fb;
    L4Re::Framebuffer::Info info;
    l4_addr_t base_addr;
    std::list<std::string> *history;
    void render();
  public:
    Console_server();
    int dispatch(l4_umword_t obj, L4::Ipc_iostream &ios);
};

#endif
