#include <l4/fb_muxer/fb_muxer.h>

#include <l4/re/env>
#include <l4/re/protocols>
#include <l4/re/service-sys.h>
#include <l4/re/util/cap_alloc>
#include <l4/re/util/fb>
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

	fbds = L4Re::Util::cap_alloc.alloc<L4Re::Dataspace>();
	if (!fbds.is_valid()) {
		printf("Could not get a valid dataspace capability for the framebuffer!\n");

		exit(1);
	}

	void* foo = (void*) &fb_addr;
	L4Re::Util::Fb::get(fb, fbds, &foo);

	if (fb->info(&info)) {
		printf("Could not get framebuffer info!\n");
	}

	vfbs = new std::list<VFB*>();
	selected = 0;
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
				VFB *vfb = new VFB(info, fbds->size());
				vfbs->push_back(vfb);

				registry.register_obj(vfb);

				ios << vfb->obj_cap();
			}
			return L4_EOK;

		// Switch to an existing virtual framebuffer for display.
		case Switch:
			{
				int which;
				ios >> which;

				std::list<VFB*>::iterator selected_iter;
				for (selected_iter = vfbs->begin(); selected_iter != vfbs->end(); selected_iter++) {
					if (--selected < 0) break;
				}

				std::list<VFB*>::iterator switch_to_iter;
				int to_go = which;
				for (switch_to_iter = vfbs->begin(); switch_to_iter != vfbs->end(); switch_to_iter++) {
					if (--to_go < 0) break;
				}

				(*selected_iter)->buffer();
				(*switch_to_iter)->write_through(fb_addr);
			}

			return L4_EOK;

		default:

			return -L4_ENOSYS;
	}
}

VFB::VFB(L4Re::Framebuffer::Info info, int size)
{
	vfb = L4Re::Util::cap_alloc.alloc<L4Re::Dataspace>();
	if (!vfb.is_valid()) {
		printf("Could not get a valid dataspace capability!\n");
		
		exit(1);
	}

	// Allocate dataspace and setup Dataspace_svr.
	L4Re::Env::env()->mem_alloc()->alloc(size, vfb);
	L4Re::Env::env()->rm()->attach(&vfb_start, size, L4Re::Rm::Search_addr, vfb, 0);
	
	_ds_start = vfb_start;
	_ds_size = size;
	_rw_flags = Writable;

	// Set up information for Framebuffer_svr.
	_fb_ds = vfb;
	_info = info;
}

VFB::~VFB() throw()
{
	// FIXME: I don't know whether it's really safe to do *nothing* here?
}

/*
Switch back to acting as a virtual framebuffer.
*/
void VFB::buffer()
{
	l4_addr_t fb_start = _ds_start;
	_ds_start = vfb_start;

	// Copy snapshot of real framebuffer into the virtual one.
	memcpy((void*) _ds_start, (void*) fb_start, vfb->size());

	// Unmap all pages in the real framebuffer.
	l4_addr_t temp = fb_start;
	while (temp < fb_start + vfb->size()) {
		l4_task_unmap(L4Re::Env::env()->task().cap(), l4_fpage(temp, 10, L4_FPAGE_RWX), L4_FP_OTHER_SPACES);
		temp += 1024 * 4096;
	}
}

/*
Forward subsequent memory accesses to the real framebuffer.
*/
void VFB::write_through(l4_addr_t start)
{
	_ds_start = start;

	// Copy snapshot of virtual framebuffer into the real one.
	memcpy((void*) _ds_start, (void*) vfb_start, vfb->size());

	// Unmap all pages in the virtual framebuffer.
	l4_addr_t temp = vfb_start;
	while (temp < vfb_start + vfb->size()) {
		l4_task_unmap(L4Re::Env::env()->task().cap(), l4_fpage(temp, 10, L4_FPAGE_RWX), L4_FP_OTHER_SPACES);
		temp += 1024 * 4096;
	}
}

int VFB::dispatch(l4_umword_t obj, L4::Ipc_iostream &ios)
{
	return L4Re::Util::Framebuffer_svr::dispatch(obj, ios);
};


int main(int argc, char **argv)
{
	FBMuxer *muxer = new FBMuxer();

	registry.register_obj(muxer);

	L4::Server<L4::Basic_registry_dispatcher> server(l4_utcb());

	if (L4Re::Env::env()->names()->register_obj("fb_muxer", muxer->obj_cap())) {
		printf("Could not register muxer, probably read-only namespace?\n");
		
		return 1;
	}

	printf("Framebuffer multiplexer started.\n");
	server.loop();

	return 0;
}
