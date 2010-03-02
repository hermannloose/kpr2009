#include <l4/fb_muxer/fb_muxer.h>

#include <l4/re/env>
#include <l4/re/protocols>
#include <l4/re/service-sys.h>
#include <l4/re/util/cap_alloc>
#include <l4/re/util/object_registry>

#include <stdio.h>
#include <stdlib.h>

static L4Re::Util::Object_registry registry(
	L4Re::Env::env()->main_thread(),
	L4Re::Env::env()->factory()
);

FBMuxer::FBMuxer()
{
	l4_msgtag_t err;

	fb = L4Re::Util::cap_alloc.alloc<L4Re::Framebuffer>();
	if (!fb.is_valid()) {
		printf("Could not get a valid capability slot!\n");
		
		exit(1);
	}
	if (L4Re::Env::env()->names()->query("fb", fb)) {
		printf("Could not get framebuffer capability!\n");

		exit(1);
	}
	if (fb->info(&info)) {
		printf("Could not get framebuffer info!\n");
	}

	vfbs = new std::list<VFB*>();

}

int FBMuxer::dispatch(l4_umword_t obj, L4::Ipc_iostream &ios)
{
	l4_msgtag_t tag;
	ios >> tag;

	switch (tag.label()) {
		
		case L4Re::Protocol::Service:
		case Switch:
			break;
		
		// Fail for any protocol not listed.
		default:
			printf("Unsupported protocol!\n");
		
			return -L4_EBADPROTO;
	}

	L4::Opcode opcode;
	ios >> opcode;

	switch (opcode) {
		
		// Create a new virtual framebuffer.
		case L4Re::Service_::Open:
			{
				VFB *vfb = new VFB(info);
				vfbs->push_back(vfb);

				registry.register_obj(vfb);

				ios << vfb->obj_cap();
			}
			return L4_EOK;

		// Switch to an existing virtual framebuffer for display.
		case Switch:

			return L4_EOK;

		default:

			return -L4_ENOSYS;
	}
}

VFB::VFB(L4Re::Framebuffer::Info info)
{
	_info = info;
}

int VFB::dispatch(l4_umword_t obj, L4::Ipc_iostream &ios)
{
	return 0;
};

int main(int argc, char **argv)
{
	FBMuxer *muxer = new FBMuxer();

	registry.register_obj(muxer);

	L4::Server<L4::Basic_registry_dispatcher> server(l4_utcb());

	if (L4Re::Env::env()->names()->register_obj("muxer", muxer->obj_cap())) {
		printf("Could not register muxer, probably read-only namespace?\n");
		
		return 1;
	}

	server.loop();

	return 0;
}
