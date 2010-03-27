#ifndef LOG2CONSOLE
#define LOG2CONSOLE

#include <l4/cxx/iostream.h>
#include <l4/cxx/ipc_server>
#include <l4/re/env>
#include <l4/re/protocols>
#include <l4/re/service-sys.h>
#include <l4/re/util/cap_alloc>
#include <l4/re/util/object_registry>
#include <l4/sys/types.h>

#include <stdio.h>
#include <stdlib.h>

class Log2Console : public L4::Server_object
{
	private:
		L4::Cap<void> console;
	public:
		Log2Console();
		int dispatch(l4_umword_t obj, L4::Ipc_iostream &ios);
};

#endif
