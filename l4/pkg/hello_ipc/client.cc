#include "client.h"
#include "shared.h"

#include <l4/re/env>
#include <l4/re/util/cap_alloc>
#include <l4/cxx/ipc_stream>
#include <x86/l4/util/util.h>

#include <stdio.h>

L4::Cap<void> server;

Hello_client::Hello_client(char const * name){
	this->name = name;
}

void Hello_client::show(char const * str){
	printf("Calling server with message [%s] ...\n", str);
	L4::Ipc_iostream s(l4_utcb());
	s << l4_umword_t(Opcode::Echo) << name << str;
	l4_msgtag_t result = s.call(server.cap(), Protocol::Hello);
	if(l4_ipc_error(result, l4_utcb())){
		printf("Call returned an error!\n");
	}
}

int main(int argc, char * argv[]){
	char * name = "";
	char * msg = "";
	if(argc = 1){
		msg = argv[1];
	}
	if(argc = 2){
		name = argv[1];
		msg = argv[2];
	}
	printf("Starting as [%s] ...\n", name);
	server = L4Re::Util::cap_alloc.alloc<void>();
	if(!server.is_valid()){
		printf("Could not get capability slot!\n");
		return 1;
	}
	if(L4Re::Env::env()->names()->query("srv", server)){
		printf("Could not get server capability!\n");
		return 1;
	}
	Hello_client client(name);
	for(int i = 0;;i++){
		client.show(msg);
		l4_sleep(1000);
	}
	return 0;
}
