#ifndef FB_MUXER
#define FB_MUXER

#include <l4/cxx/ipc_server>
#include <l4/sys/types.h>

class FBMuxer : public L4::Server_object
{
	public:
		FBMuxer();
		int dispatch(l4_umword_t obj, L4::Ipc_iostream ios);
};

#endif
