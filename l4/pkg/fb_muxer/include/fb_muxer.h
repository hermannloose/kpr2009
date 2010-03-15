#ifndef FB_MUXER
#define FB_MUXER

#include <l4/cxx/iostream.h>
#include <l4/cxx/ipc_server>
#include <l4/re/fb>
#include <l4/re/util/dataspace_svr>
#include <l4/re/util/framebuffer_svr>
#include <l4/sys/types.h>

#include <list>

static const int Switch = 0xf00;

class FBMuxer;
class VFB;
class VFB_fb_svr;

class FBMuxer : public L4::Server_object
{
	private:
		// Physical framebuffer
		L4::Cap<L4Re::Framebuffer> fb;
		L4::Cap<L4Re::Dataspace> fbds;
		l4_addr_t fb_addr;
		L4Re::Framebuffer::Info info;
		// Virtual framebuffers
		std::list<VFB*> *vfbs;
		int selected;

		int switch_to(int which);
	public:
		FBMuxer();
		int dispatch(l4_umword_t obj, L4::Ipc_iostream &ios);
};


class VFB : public L4::Server_object, public L4Re::Util::Dataspace_svr
{
	private:
		L4::Cap<L4Re::Dataspace> vfb;
		l4_addr_t vfb_start;
	public:
		VFB_fb_svr *fbsvr;
		VFB(L4Re::Framebuffer::Info info, int size);
		~VFB() throw();
		void buffer();
		void write_through(l4_addr_t start);
		int dispatch(l4_umword_t obj, L4::Ipc_iostream &ios);
};

class VFB_fb_svr : public L4::Server_object, public L4Re::Util::Framebuffer_svr
{
	public:
		VFB_fb_svr(L4Re::Framebuffer::Info, L4::Cap<L4Re::Dataspace>);
		int dispatch(l4_umword_t obj, L4::Ipc_iostream &ios);
};

#endif
