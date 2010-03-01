#ifndef FB_MUXER
#define FB_MUXER

#include <l4/cxx/ipc_server>
#include <l4/re/fb>
#include <l4/sys/types.h>

static const int Switch = 0xf00;

class FBMuxer : public L4::Server_object
{
	private:
		L4::Cap<L4Re::Framebuffer> fb;
	public:
		FBMuxer();
		int dispatch(l4_umword_t obj, L4::Ipc_iostream ios);
};


#endif
