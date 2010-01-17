#ifndef HELLO_SERVER
#define HELLO_SERVER

#include <l4/sys/types.h>
#include <l4/cxx/ipc_server>
#include <l4/cxx/iostream.h>
#include <l4/re/util/object_registry>

class Hello_server : public L4::Server_object{
	public:
		Hello_server();
		int dispatch(l4_umword_t obj, L4::Ipc_iostream &ios);
};

class Worker : public L4::Server_object{
	public:
		int dispatch(l4_umword_t obj, L4::Ipc_iostream &ios);
};

#endif
