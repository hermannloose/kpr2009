#include <l4/console/console.h>

#include <l4/libgfxbitmap/font.h>

#include <l4/re/fb>
#include <l4/re/env>
#include <l4/re/protocols>
#include <l4/re/service-sys.h>
#include <l4/re/util/cap_alloc>
#include <l4/re/util/object_registry>
#include <l4/sys/types.h>

#include <iostream>
#include <list>
#include <string>

#include <stdio.h>
#include <stdlib.h>

Console_server::Console_server()
{
  fb = L4Re::Util::cap_alloc.alloc<L4Re::Framebuffer>();
  if(!fb.is_valid()){
    std::cerr << "Could not get capability slot!" << std::endl;
    exit(1);
  }
  if(L4Re::Env::env()->names()->query("fb", fb)){
    std::cerr << "Could not get framebuffer capability!" << std::endl;
    exit(1);
  }
  if(fb->info(&info)){
    std::cerr << "Could not get framebuffer info!" << std::endl;
    exit(1);
  }
  L4::Cap<L4Re::Dataspace> ds = L4Re::Util::cap_alloc.alloc<L4Re::Dataspace>();
  if(!ds.is_valid()){
    std::cerr << "Could not get dataspace capability!" << std::endl;
    exit(1);
  }
  fb->mem(ds);
  base_addr = 0;
  if(L4Re::Env::env()->rm()->attach(&base_addr, ds->size(), L4Re::Rm::Search_addr, ds, 0)){
    std::cerr << "Could not attach dataspace in region map!" << std::endl;
    exit(1);
  }
  
  gfxbitmap_font_init();
  font = (void*) GFXBITMAP_DEFAULT_FONT;
  font_height = gfxbitmap_font_height((void*) GFXBITMAP_DEFAULT_FONT);
  font_width = gfxbitmap_font_width((void*) GFXBITMAP_DEFAULT_FONT);
  lines = info.y_res / gfxbitmap_font_height((void*) GFXBITMAP_DEFAULT_FONT);
  chars = info.x_res / gfxbitmap_font_width((void*) GFXBITMAP_DEFAULT_FONT);
  window_start = 0;

  history = new std::list<std::string>();
  history->push_back("Console started.");
  history->push_back("Hello World!");
  render();
  // for testing
  window_start = 1;
}

int Console_server::dispatch(l4_umword_t obj, L4::Ipc_iostream &ios)
{
	printf("Console %p: dispatching.\n", this);
  l4_msgtag_t tag;
  ios >> tag;
	printf("Console %p: tag label [%i].\n", this, tag.label());

	if (tag.label() == L4Re::Protocol::Service) {
		printf("Console %p: opening service (dummy).\n", this);
		L4::Opcode opcode;
		ios >> opcode;
		if (opcode == L4Re::Service_::Open) {
			ios << this;
			return L4_EOK;
		} else {
			return -L4_ENOSYS;
		}
	}

  if(tag.label() != Protocol::Console){
		return -L4_EBADPROTO;
  }

  L4::Opcode opcode;
  ios >> opcode;
	printf("Console %p: opcode [%i].\n", this, opcode);
  switch(opcode){
    case Opcode::Put:
			printf("Console %p: receiving input.\n", this);
			char *msg;
			unsigned long int msg_size;
			ios >> L4::Ipc_buf_in<char>(msg, msg_size);
      std::cout << "Received: [" << msg << "]" << std::endl;
			history->push_back(msg);
			render();
      return L4_EOK;
    case Opcode::Scroll:
      // TODO Get the value for scrolling from ios and scroll.
      return L4_EOK;
    default:
      return -L4_ENOSYS;
  }
  return 0;
}

void Console_server::render()
{
  int x = 1;
  int y = 1 + font_height;
  int line = 0;
  std::cout << "=== Rendering ===" << std::endl;
  std::list<std::string>::iterator iter;
  for(iter = history->begin(); iter != history->end(); iter++){
    std::cout << *iter << std::endl;
    gfxbitmap_font_text(
      &base_addr,
      (l4re_fb_info_t*) &info,
      (void*) GFXBITMAP_DEFAULT_FONT,
      iter->c_str(), GFXBITMAP_USE_STRLEN,
      x, y,
      0xffffff, 0
    );
    y += font_height + 1;
    line += 1;
    if(y > info.y_res || line >= lines) break;
  }
  std::cout << "=== Finished! ===" << std::endl;
}

int main(int argc, char **argv)
{
  std::cout << "Starting console server ..." << std::endl;
  L4::Server<L4::Basic_registry_dispatcher> server(l4_utcb());
  L4Re::Util::Object_registry registry(L4Re::Env::env()->main_thread(), L4Re::Env::env()->factory());
  Console_server *console = new Console_server();
  registry.register_obj(console);
  if(L4Re::Env::env()->names()->register_obj("console", console->obj_cap())){
    std::cerr << "Could not register service, probably read-only namespace." << std::endl;
    return 1;
  }
  std::cout << "Console server started." << std::endl;
  server.loop();
  return 0;
}
