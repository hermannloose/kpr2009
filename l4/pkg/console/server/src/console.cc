#include <l4/console/console.h>

#include <l4/libgfxbitmap/font.h>

#include <l4/re/fb>
#include <l4/re/env>
#include <l4/re/util/cap_alloc>
#include <l4/re/util/object_registry>
#include <l4/sys/types.h>

#include <stdio.h>
#include <stdlib.h>

Console_server::Console_server()
{
  fb = L4Re::Util::cap_alloc.alloc<L4Re::Framebuffer>();
  if(!fb.is_valid()){
    printf("Could not get capability slot!\n");
    exit(1);
  }
  if(L4Re::Env::env()->names()->query("fb", fb)){
    printf("Could not get framebuffer capability!\n");
    exit(1);
  }
  L4Re::Framebuffer::Info info;
  if(fb->info(&info)){
    printf("Could not get framebuffer info!\n");
    exit(1);
  }
  printf("Framebuffer size: %i x %i.\n", info.x_res, info.y_res);
  L4::Cap<L4Re::Dataspace> ds = L4Re::Util::cap_alloc.alloc<L4Re::Dataspace>();
  if(!ds.is_valid()){
    printf("Could not get dataspace capability!\n");
    exit(1);
  }
  fb->mem(ds);
  base_addr = 0;
  if(L4Re::Env::env()->rm()->attach(&base_addr, ds->size(), L4Re::Rm::Search_addr, ds, 0)){
    printf("Could not attach dataspace in region map!\n");
    exit(1);
  }
  gfxbitmap_font_init();
  // Hello World!
  gfxbitmap_font_text(
    &base_addr,
    (l4re_fb_info_t*) &info,
    (void*) GFXBITMAP_DEFAULT_FONT,
    "Hello World!", GFXBITMAP_USE_STRLEN,
    10, 10 + gfxbitmap_font_height((void*) GFXBITMAP_DEFAULT_FONT),
    0xffffff,
    0
  );
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
  printf("Console server started.\n");
  server.loop();
  return 0;
}
