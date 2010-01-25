#include "server.h"
#include "shared.h"

#include <l4/sys/types.h>
#include <l4/re/env>
#include <l4/re/protocols>
#include <l4/re/service-sys.h>
#include <l4/re/util/cap_alloc>
#include <l4/re/util/object_registry>

#include <stdio.h>
#include <iostream>
#include <sstream>
#include <string>

int client_no = 0;

static L4Re::Util::Object_registry registry(L4Re::Env::env()->main_thread(), L4Re::Env::env()->factory());

Hello_server::Hello_server()
{
  //registry.register_obj(this);
}

int Hello_server::dispatch(l4_umword_t obj, L4::Ipc_iostream &ios){
	printf("Hello_server is dispatching ...\n");
	l4_msgtag_t tag;
	ios >> tag;
	if(tag.label() != L4Re::Protocol::Service){
		return -L4_EBADPROTO;
	}
	L4::Opcode opcode;
	ios >> opcode;
	switch(opcode){
		case L4Re::Service_::Open:
			{
			// Allocate client cap
			std::stringstream s;
			s << "client" << ++client_no;
			Worker *worker = new Worker(s.str());
			registry.register_obj(worker);
			// Put client cap back into ios
			ios << worker->obj_cap();
			}
			return L4_EOK;
		default:
			return -L4_ENOSYS;
	}
	return 0;
}

Worker::Worker(std::string name)
{
  this->name = name;
}

int Worker::dispatch(l4_umword_t obj, L4::Ipc_iostream &ios)
{
	//printf("Worker is dispatching ...\n");
	l4_msgtag_t tag;
	ios >> tag;
	if (tag.label() != Protocol::Hello){
		return -L4_EBADPROTO;
	}
	//printf("IPC payload is %i words.\n", tag.words());
	L4::Opcode opcode;
	ios >> opcode;
	unsigned long int name_size = 32;
	char name[name_size];
	L4::Ipc_buf_cp_in<char> name_buf(name, name_size);
	ios >> name_buf;
	l4_umword_t content;
	unsigned long int msg_size = 256;
	char msg[msg_size];
	L4::Ipc_buf_cp_in<char> msg_buf(msg, msg_size);
	switch(opcode){
		// After inserting the transmission of the client's "name" into the protocol
		// I couldn't be bothered to fix the loop for the dump, so it might give
		// strange results due to the "i < tag.words()-1" part ...
		case Opcode::Dump:
			for(int i = 0; i < tag.words()-1; i++){
				ios >> content;
				printf("%04i:%08x\n", i, content);
			}
			return L4_EOK;
		case Opcode::Echo:
			ios >> msg_buf;
			std::cout << "[" << this->name << "] " << msg << std::endl;
			return L4_EOK;
		default:
			return -L4_ENOSYS;
	}
	return 0;
}

int main(){
	printf("Starting ...\n");
	L4::Server<L4::Basic_registry_dispatcher> server(l4_utcb());
	Hello_server *hello = new Hello_server;
	printf("Created new Hello_server.\n");
	registry.register_obj(hello);
	if (L4Re::Env::env()->names()->register_obj("service", hello->obj_cap())){
		printf("Could not register service, probably read-only namespace.\n");
		return 1;
	}
	printf("Listening ...\n");
	server.loop();
	return 0;
}
