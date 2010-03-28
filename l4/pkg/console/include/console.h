#ifndef CONSOLE_H
#define CONSOLE_H

#include <l4/cxx/iostream.h>
#include <l4/cxx/ipc_server>
#include <l4/libgfxbitmap/font.h>
#include <l4/re/fb>
#include <l4/re/protocols>
#include <l4/re/util/cap_alloc>
#include <l4/sys/types.h>

#include <list>
#include <string>

namespace Protocol
{
	enum Protocols
	{
		Console
	};
};

namespace Opcode
{
	enum Opcodes
	{
		Put, Refresh, Scroll
	};
};

enum Scrolling
{
	LINE_UP, LINE_DOWN, PAGE_UP, PAGE_DOWN, TOP, BOTTOM
};

class Console_server : public L4::Server_object
{
	private:
		L4::Cap<L4Re::Framebuffer> fb;
		L4Re::Framebuffer::Info info;
		L4::Cap<L4Re::Dataspace> ds;
		// gfx bitmap variables
		l4re_fb_info_t fbi;
		gfxbitmap_color_pix_t pixcol;
		gfxbitmap_font_t font;
		int font_height;
		int font_width;
		int lines;
		int chars;
		int window_start;
		l4_addr_t base_addr;
		std::list<std::string> *history;
		void clear();
		void render();
	public:
		Console_server();
		Console_server(std::string bootmsg);
		int dispatch(l4_umword_t obj, L4::Ipc_iostream &ios);
};

#endif
