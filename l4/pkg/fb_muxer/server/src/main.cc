#include <l4/fb_muxer/fb_muxer.h>

#include <l4/re/env>
#include <l4/re/protocols>
#include <l4/re/service-sys.h>
#include <l4/re/util/cap_alloc>
#include <l4/re/util/fb>
#include <l4/re/util/object_registry>
#include <l4/sys/capability>

#include <assert.h>
#include <math.h>
#include <stdio.h>
#include <stdlib.h>

#define debug 1

static L4Re::Util::Object_registry registry(
	L4Re::Env::env()->main_thread(),
	L4Re::Env::env()->factory()
);

FBMuxer::FBMuxer()
{
	l4_msgtag_t err;

	fb_addr = 0;

	// Get the initial framebuffer.
	fb = L4Re::Util::cap_alloc.alloc<L4Re::Framebuffer>();
	
	if (!fb.is_valid()) {
		printf("Could not get a valid capability slot!\n");
		
		exit(1);
	}
	
	if (L4Re::Env::env()->names()->query("fb", fb)) {
		printf("Could not get framebuffer!\n");

		exit(1);
	}

	// Get framebuffer info.
	if (fb->info(&info)) {
		printf("Could not get framebuffer info!\n");

		exit(1);
	}

	// Get framebuffer memory.
	fbds = L4Re::Util::cap_alloc.alloc<L4Re::Dataspace>();
	
	if (!fbds.is_valid()) {
		printf("Could not get a valid dataspace capability for the framebuffer!\n");

		exit(1);
	}

	// Make framebuffer memory available to the multiplexer.
	fb->mem(fbds);

	if (L4Re::Env::env()->rm()->attach(&fb_addr, fbds->size(), L4Re::Rm::Search_addr, fbds, 0)) {
		printf("Could not attach dataspace in region map!\n");

		exit(1);
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
			printf("ERROR: Unsupported protocol!\n");
		
			return -L4_EBADPROTO;
	}

	L4::Opcode opcode;
	ios >> opcode;

	switch (opcode) {
		
		// Create a new virtual framebuffer.
		case L4Re::Service_::Open:
			printf("Opening virtual framebuffer.\n");
			{
				#if debug
				printf("Creating new VFB.\n");
				#endif
				VFB *vfb = new VFB(info, fbds->size());
				
				vfbs->push_back(vfb);

				if (vfbs->size() == 1) {
					vfb->write_through(fb_addr);
				}

				ios << vfb->fbsvr->obj_cap();
			}
			printf("Created new virtual framebuffer.\n");

			return L4_EOK;

		// Switch to an existing virtual framebuffer for display.
		case Switch:
			{
				int which;
				ios >> which;
				switch_to(which);
			}

			return L4_EOK;

		default:
			printf("ERROR: Unsupported opcode!\n");

			return -L4_ENOSYS;
	}
}

int FBMuxer::switch_to(int which)
{
	// FIXME Dirty.
	assert((0 <= which) && (which < vfbs->size()));

	printf("Switching to virtual framebuffer %i for display.\n", which);

	int old = selected;

	std::list<VFB*>::iterator selected_iter;
	for (selected_iter = vfbs->begin(); selected_iter != vfbs->end(); selected_iter++) {
		if (--selected < 0) break;
	}
	selected = which;

	#if debug
	printf("Got old VFB.\n");
	#endif

	std::list<VFB*>::iterator switch_to_iter;
	int to_go = which;
	for (switch_to_iter = vfbs->begin(); switch_to_iter != vfbs->end(); switch_to_iter++) {
		if (--to_go < 0) break;
	}

	#if debug
	printf("Got new VFB.\n");
	#endif

	(*selected_iter)->buffer();
	(*switch_to_iter)->write_through(fb_addr);

	printf("Switched buffers.\n");

	// Ensure "selected" is up-to-date.
	assert(selected == which);

	return old;
}

VFB::VFB(L4Re::Framebuffer::Info info, int size)
{
	sem_init(&memaccess, 0, 1);

	#if debug
	printf("Creating new VFB.\n");
	#endif
	vfb_start = 0;
	
	vfb = L4Re::Util::cap_alloc.alloc<L4Re::Dataspace>();
	if (!vfb.is_valid()) {
		printf("Could not get a valid dataspace capability!\n");
		
		exit(1);
	}
	#if debug
	printf("Got dataspace capability.\n");
	#endif

	// Allocate dataspace and setup Dataspace_svr.
	if (L4Re::Env::env()->mem_alloc()->alloc(size, vfb)) {
		printf("Could not allocate memory for VFB dataspace!\n");

		exit(1);
	}
	#if debug
	printf("Got dataspace memory.\n");
	#endif

	if (L4Re::Env::env()->rm()->attach(&vfb_start, size, L4Re::Rm::Search_addr, vfb, 0)) {
		printf("Could not attach dataspace memory to region map!\n");

		exit(1);
	}
	#if debug
	printf("Attached dataspace @ %p.\n", (void*) vfb_start);
	#endif

	#if debug
	printf("Zeroing %i bytes from %p.\n", size, (void*) vfb_start);
	#endif
	memset((void*) vfb_start, 0, size);

	_ds_start = vfb_start;
	_ds_size = size;
	_rw_flags = Writable;

	registry.register_obj(this);

	fbsvr = new VFB_fb_svr(info, this->obj_cap());
}

VFB::~VFB() throw()
{
	// FIXME: I don't know whether it's really safe to do *nothing* here?
}

// Switch back to acting as a virtual framebuffer.
void VFB::buffer()
{
	sem_wait(&memaccess);

	l4_addr_t fb_start = _ds_start;
	_ds_start = vfb_start;

	printf("VFB @ %p buffering:\n  old DS @ %p\n  new DS @ %p\n", (void*) this, (void*) fb_start, (void*) _ds_start);

	// Unmap all pages in the real framebuffer.
	l4_addr_t temp = fb_start;
	
	#if debug
	printf("unmap %p -> %p.\n", (void*) fb_start, (void*) (fb_start + vfb->size()));
	#endif
	
	while (temp < fb_start + vfb->size()) {
		l4_task_unmap(L4Re::Env::env()->task().cap(), l4_fpage(temp, 21, L4_FPAGE_RWX), L4_FP_OTHER_SPACES);
		temp += 1024 * 2048;
	}

	// Copy snapshot of real framebuffer into the virtual one.
	memcpy((void*) _ds_start, (void*) fb_start, vfb->size());
	
	#if debug
	printf("memcpy %p -> %p (%i bytes)\n", (void*) fb_start, (void*) _ds_start, vfb->size());
	#endif

	sem_post(&memaccess);
}

// Forward subsequent memory accesses to the real framebuffer.
void VFB::write_through(l4_addr_t start)
{
	sem_wait(&memaccess);
	
	_ds_start = start;

	printf("VFB @ %p forwarding:\n  old DS @ %p\n  new DS @ %p\n", (void*) this, (void*) vfb_start, (void*) _ds_start);

	// Unmap all pages in the virtual framebuffer.
	l4_addr_t temp = vfb_start;
	
	#if debug
	printf("unmap %p -> %p.\n", (void*) vfb_start, (void*) (vfb_start + vfb->size()));
	#endif

	while (temp < vfb_start + vfb->size()) {
		l4_task_unmap(L4Re::Env::env()->task().cap(), l4_fpage(temp, 21, L4_FPAGE_RWX), L4_FP_OTHER_SPACES);
		temp += 1024 * 2048;
	}

	// Copy snapshot of virtual framebuffer into the real one.
	memcpy((void*) _ds_start, (void*) vfb_start, vfb->size());
	
	#if debug
	printf("memcpy %p -> %p (%i bytes)\n", (void*) vfb_start, (void*) _ds_start, vfb->size());
	#endif

	sem_post(&memaccess);
}

// Forward to the default implementation.
int VFB::dispatch(l4_umword_t obj, L4::Ipc_iostream &ios)
{
	#if debug
	printf("Dataspace_svr dispatching.\n");
	#endif

	// This will still allow race conditions, albeit fewer.
	sem_wait(&memaccess);
	sem_post(&memaccess);

	return L4Re::Util::Dataspace_svr::dispatch(obj, ios);
};

//VFB_fb_svr::VFB_fb_svr(L4Re::Framebuffer::Info info, L4::Cap<L4Re::Dataspace> vfb)
VFB_fb_svr::VFB_fb_svr(L4Re::Framebuffer::Info info, L4::Cap<void> vfb)
{
	#if debug
	printf("Creating new VFB_fb_svr.\n");
	#endif

	// Set up information for Framebuffer_svr.
	_fb_ds = L4::cap_cast<L4Re::Dataspace>(vfb);
	_info = info;

	registry.register_obj(this);
}

// Forward to the default implementation.
int VFB_fb_svr::dispatch(l4_umword_t obj, L4::Ipc_iostream &ios)
{
	#if debug
	printf("Framebuffer_svr dispatching.\n");
	#endif

	return L4Re::Util::Framebuffer_svr::dispatch(obj, ios);
}

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
