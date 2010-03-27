#include <l4/console/console.h>
#include <l4/log2console/log2console.h>

#include <string>

static L4Re::Util::Object_registry registry(
	L4Re::Env::env()->main_thread(),
	L4Re::Env::env()->factory()
);

Log2Console::Log2Console()
{
	l4_msgtag_t err;

	console = L4Re::Util::cap_alloc.alloc<void>();
	if (!console.is_valid()) {
		printf("ERROR: Could not get a valid void capability!\n");

		exit(1);
	}
	if (L4Re::Env::env()->names()->query("console", console)) {
		printf("ERROR: Could not get console capability!\n");

		exit(1);
	}

}

int Log2Console::dispatch(l4_umword_t obj, L4::Ipc_iostream &ios)
{
	l4_msgtag_t tag;
	ios >> tag;

	L4::Opcode opcode;

	if (tag.label() == L4Re::Protocol::Service) {
		printf("Opening service.\n");
		
		ios >> opcode;

		if (opcode != L4Re::Service_::Open) {
			printf("ERROR: Unsupported opcode %i!\n", opcode);

			return -L4_ENOSYS;
		} else {
			ios << this->obj_cap();

			return L4_EOK;
		}
	}

	ios >> opcode;

	if (opcode != 0) {
		printf("ERROR: Unsupported opcode %i!\n", opcode);

		return -L4_ENOSYS;
	} else {
		int length;
		ios >> length;
		printf("Message length: %i\n", length);

		std::string output;
		char actual;
		for (int i = 0; i < length; i++) {
			ios >> actual;
			output += actual;
		}

		ios.reset();
		ios << L4::Opcode(Opcode::Put) << output.c_str();
		ios.call(console.cap(), Protocol::Console);

		return L4_EOK;
	}
}

int main(int argc, char **argv)
{
	l4_msgtag_t err;

	Log2Console *log2console = new Log2Console();

	L4::Server<L4::Basic_registry_dispatcher> server(l4_utcb());

	registry.register_obj(log2console);

	if (L4Re::Env::env()->names()->register_obj("log", log2console->obj_cap())) {
		printf("ERROR: Could not register service, probably read-only namespace?\n");

		exit(1);
	}

	server.loop();

  return 0;
}
