#include <l4/console/console.h>

#include <l4/re/env>
#include <l4/re/util/cap_alloc>
#include <l4/re/util/object_registry>
#include <l4/sys/types.h>

#include <stdio.h>

Console_server::Console_server()
{

}

int Console_server::dispatch(l4_umword_t obj, L4::Ipc_iostream &ios)
{
  return 0;
}

int main(int argc, char **argv)
{
  printf("Starting console server ...\n");
  L4::Server<L4::Basic_registry_dispatcher> server(l4_utcb());
  L4Re::Util::Object_registry registry(L4Re::Env::env()->main_thread(), L4Re::Env::env()->factory());
  Console_server *console = new Console_server();
  registry.register_obj(console);
  if(L4Re::Env::env()->names()->register_obj("console", console->obj_cap())){
    printf("Could not register service, probably read-only namespace.\n");
    return 1;
  }
  return 0;
}
