#include <l4/console/console.h>

#include <l4/libgfxbitmap/font.h>

#include <l4/re/c/fb.h>
#include <l4/re/fb>
#include <l4/re/env>
#include <l4/re/protocols>
#include <l4/re/service-sys.h>
#include <l4/re/util/cap_alloc>
#include <l4/re/util/object_registry>
#include <l4/sys/types.h>

#include <assert.h>
#include <iostream>
#include <list>
#include <sstream>
#include <string>
#include <string.h>

#include <stdio.h>
#include <stdlib.h>

#define debug 1

Console_server::Console_server()
{
	Console_server::Console_server("Console started.");
}

Console_server::Console_server(std::string bootmsg)
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
	ds = L4Re::Util::cap_alloc.alloc<L4Re::Dataspace>();
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

	#if debug
	printf("Initialized framebuffer.\n"); 
	#endif

	fbi = *((l4re_fb_info_t*) &info);

	pixcol = gfxbitmap_convert_color(&fbi, 0xe07000);

	gfxbitmap_font_init();
	font = (void*) GFXBITMAP_DEFAULT_FONT;
	font_height = gfxbitmap_font_height((void*) GFXBITMAP_DEFAULT_FONT);
	font_width = gfxbitmap_font_width((void*) GFXBITMAP_DEFAULT_FONT);
	//lines = info.y_res / gfxbitmap_font_height((void*) GFXBITMAP_DEFAULT_FONT);
	lines = 50;
	chars = info.x_res / gfxbitmap_font_width((void*) GFXBITMAP_DEFAULT_FONT);

	#if debug
	printf("Console: approximately %i lines x %i chars\n", lines, chars);
	#endif

	#if debug
	printf("Rendering boot message.\n");
	#endif

	history = new std::list<std::string>();
	history->push_back(bootmsg);
	window_start = 0;
	window_end = 1;
	
	render();

	follow = FOLLOW;
}

void Console_server::print(std::string msg)
{
	int window_size = window_end - window_start;
	int old_window_size = window_size;
	//std::stringstream s;
	//s << history->size() << ": " << msg;
	
	//history->push_back(s.str());
	history->push_back(msg);
	
	if ((window_end - window_start) < lines) {
		window_end++;
	}

	if (follow == FOLLOW) scroll(LINE_DOWN);

	assert(window_start != window_end);
	assert(window_size >= old_window_size);
}

int Console_server::dispatch(l4_umword_t obj, L4::Ipc_iostream &ios)
{
	l4_msgtag_t tag;
	ios >> tag;

	if (tag.label() == L4Re::Protocol::Service) {
		printf("Console %p: opening service (dummy).\n", this);
		L4::Opcode opcode;
		ios >> opcode;
		
		if (opcode == L4Re::Service_::Open) {
			ios << this->obj_cap();
			
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
	
	switch(opcode){
		
		case Opcode::Put:
			{
			char *msg;
			unsigned long int msg_size;
			ios >> L4::Ipc_buf_in<char>(msg, msg_size);
			std::string str(msg);

			// Strip newlines, as they will not display properly.
			size_t substr_start = 0;
			size_t substr_end = str.npos;
			size_t substr_length = 0;
			while (1) {
				#if debug
				std::cout << "start " << substr_start << " end " << substr_end << " npos " << str.npos << std::endl;
				#endif
			
				substr_end = str.find("\n", substr_start);

				substr_length = substr_end - substr_start;

				#if debug
				std::cout << "start " << substr_start << " end " << substr_end << " npos " << str.npos << std::endl;
				#endif
				
				print(str.substr(substr_start, substr_length));

				if (substr_end < str.npos) {
					substr_start = substr_end + 1;
					if (substr_start >= str.size()) break;
				} else {
					break;
				}
			}

			render();
			}

			return L4_EOK;

		case Opcode::Refresh:
			render();

			return L4_EOK;

		case Opcode::Scroll:
			
			int mode;
			ios >> mode;
		
			scroll(mode);

			render();
			
			return L4_EOK;
		
		default:
			
			return -L4_ENOSYS;
	}
	return 0;
}

void Console_server::clear()
{
	int color = 0;
	#if debug
	printf("Clear %p to %p with %06x.\n", base_addr, base_addr + ds->size(), color);
	#endif
	memset((void*) base_addr, color, ds->size());
}

void Console_server::render()
{
	#if debug
	std::cout << "=== Rendering ===" << std::endl;
	std::cout << "Window: " << window_start << " to " << window_end << std::endl;
	#endif
	// Start with some padding around the text.
	int x = 1;
	int y = 1;

	std::list<std::string>::const_iterator iter = history->begin();
	// FF to actual view
	for (int i = 0; i < window_end; i++) {
		if (i < window_start) {
			std::cout << "At " << i << " [" << *iter << "], forwarding to " << window_start << std::endl;
			++iter;
			continue;
		} else {
			#if debug
			std::cout << "[" << *iter << "]" << std::endl;
			#endif

			gfxbitmap_font_text(
				(void*) base_addr,
				&fbi,
				(void*) GFXBITMAP_DEFAULT_FONT,
				iter->c_str(), GFXBITMAP_USE_STRLEN,
				x, y,
				pixcol, 0
			);

			y += font_height + 1;
			if((y + font_height) > info.y_res) break;
			++iter;
		}
	}
	#if debug
	std::cout << std::endl << "=== Finished! ===" << std::endl;
	#endif
}

void Console_server::scroll(int mode)
{
	int window_size = window_end - window_start;
	switch (mode) {
		case TOP:
			window_start = 0;
			window_end = window_start + window_size;
			clear();
			follow = NO_FOLLOW;
			break;
		case BOTTOM:
			window_end = history->size();
			window_start = window_end - window_size;
			if (window_start < 0) window_start = 0;
			clear();
			break;
		case LINE_UP:
			if (window_start > 0) {
				--window_start;
				--window_end;
				clear();
			}
			follow = NO_FOLLOW;
			break;
		case LINE_DOWN:
			if (window_end != history->size()) {
				++window_start;
				++window_end;
				clear();
			}
			break;
		case PAGE_UP:
			if (window_start >= window_size) {
				window_start -= window_size;
				window_end -= window_size;
				clear();
			}
			follow = NO_FOLLOW;
			break;
		case PAGE_DOWN:
			if ((history->size() - window_size) >= window_size) {
				window_start += window_size;
				window_end += window_size;
				clear();
			}
			break;
		default:
			break;
	}
	if (window_end == history->size()) follow = FOLLOW;
}

int main(int argc, char **argv)
{
	std::cout << "Starting console server ..." << std::endl;
	
	L4::Server<L4::Basic_registry_dispatcher> server(l4_utcb());
	L4Re::Util::Object_registry registry(L4Re::Env::env()->main_thread(), L4Re::Env::env()->factory());
	
	Console_server *console = new Console_server(std::string(argv[1]));

	registry.register_obj(console);
	
	if(L4Re::Env::env()->names()->register_obj("console", console->obj_cap())){
		std::cerr << "Could not register service, probably read-only namespace." << std::endl;
		
		return 1;
	}
	
	std::cout << "Console server started." << std::endl;
	server.loop();

	return 0;
}
