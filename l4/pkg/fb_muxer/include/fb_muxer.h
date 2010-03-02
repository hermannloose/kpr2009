#ifndef FB_MUXER
#define FB_MUXER

#include <l4/cxx/iostream.h>
#include <l4/cxx/ipc_server>
#include <l4/re/fb>
#include <l4/re/util/framebuffer_svr>
#include <l4/sys/types.h>

#include <list>

static const int Switch = 0xf00;

class FBMuxer;
class VFB;

class FBMuxer : public L4::Server_object
{
	private:
		L4::Cap<L4Re::Framebuffer> fb;
		L4Re::Framebuffer::Info info;
		std::list<VFB*> *vfbs;
	public:
		FBMuxer();
		int dispatch(l4_umword_t obj, L4::Ipc_iostream &ios);
};


class VFB : public L4::Server_object, public L4Re::Util::Framebuffer_svr
{
	public:
		VFB(L4Re::Framebuffer::Info info);
		int dispatch(l4_umword_t obj, L4::Ipc_iostream &ios);
};

#endif
