#include <l4/fb_muxer/fb_muxer.h>

#include <l4/re/env>
#include <l4/re/protocols>
#include <l4/re/service-sys.h>
#include <l4/re/util/cap_alloc>
#include <l4/re/util/fb>
#include <l4/re/util/object_registry>

#include <assert.h>
#include <math.h>
#include <stdio.h>
#include <stdlib.h>

#define debug 1

static L4Re::Util::Object_registry registry(
	L4Re::Env::env()->main_thread(),
	L4Re::Env::env()->factory()
);
/*
void unmap_region(void* start, void* end)
{
	assert(start < end);
	void* temp = start;
	int diff = 0;
	int unmap_size = 0;
	while (temp < end) {
		diff = temp - end;
		unmap_size = floor(log(diff) / log(2));
		#if debug
		printf("Unmapping: 2^%i bytes from %p.\n", unmap_size, temp);
		#endif
		l4_task_unmap(L4Re::Env::env()->task().cap(), l4_fpage(temp, unmap_size, L4_FPAGE_RWX), L4_FP_OTHER_SPACES);
		temp += pow(2, unmap_size);
	}
}
*/
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

	#if debug
	printf("real FB @ %p\n", fb_addr);
	#endif

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
			printf("Opening virtual framebuffer.\n");
			{
				#if debug
				printf("Creating new VFB.\n");
				#endif
				VFB *vfb = new VFB(info, fbds->size());
				#if debug
				printf("Pushing VFB to list.\n");
				#endif
				vfbs->push_back(vfb);

				registry.register_obj(vfb);
				registry.register_obj(vfb->fbsvr);

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
	printf("Attached dataspace memory to region map.\n");
	#endif

	#if debug
	printf("Zeroing %i bytes from %p.\n", size, vfb_start);
	#endif
	memset((void*) vfb_start, 0, size);

	_ds_start = vfb_start;
	_ds_size = size;
	_rw_flags = Writable;

	fbsvr = new VFB_fb_svr(info, vfb);
}

VFB::~VFB() throw()
{
	// FIXME: I don't know whether it's really safe to do *nothing* here?
}

// Switch back to acting as a virtual framebuffer.
void VFB::buffer()
{
	l4_addr_t fb_start = _ds_start;
	_ds_start = vfb_start;

	#if debug
	printf("VFB %p buffer(): old DS @ %p new DS @ %p\n", this, fb_start, _ds_start);
	#endif

	// Copy snapshot of real framebuffer into the virtual one.
	memcpy((void*) _ds_start, (void*) fb_start, vfb->size());
	
	#if debug
	printf("memcpy %p -> %p (%i bytes)\n", fb_start, _ds_start, vfb->size());
	#endif

	// Unmap all pages in the real framebuffer.
	l4_addr_t temp = fb_start;
	
	#if debug
	printf("Unmap %p - %p.\n", fb_start, fb_start + vfb->size());
	#endif
	
	while (temp < fb_start + vfb->size()) {
	
		#if debug
		printf("Unmapping page @ %p size %i\n", temp, l4_fpage_size(l4_fpage(temp, 21, L4_FPAGE_RWX)));
		#endif

		l4_task_unmap(L4Re::Env::env()->task().cap(), l4_fpage(temp, 21, L4_FPAGE_RWX), L4_FP_OTHER_SPACES);
		temp += 1024 * 2048;
	}
}

// Forward subsequent memory accesses to the real framebuffer.
void VFB::write_through(l4_addr_t start)
{
	_ds_start = start;

	#if debug
	printf("VFB %p write_through(): old DS @ %p new DS @ %p\n", this, vfb_start, _ds_start);
	#endif

	// Copy snapshot of virtual framebuffer into the real one.
	memcpy((void*) _ds_start, (void*) vfb_start, vfb->size());
	
	#if debug
	printf("memcpy %p -> %p (%i bytes)\n", vfb_start, _ds_start, vfb->size());
	#endif

	// Unmap all pages in the virtual framebuffer.
	//unmap_region(vfb_start, vfb_start + vfb->size());
	l4_addr_t temp = vfb_start;
	
	#if debug
	printf("Unmap %p - %p.\n", vfb_start, vfb_start + vfb->size());
	#endif

	while (temp < vfb_start + vfb->size()) {
		
		#if debug
		printf("Unmapping page @ %p size %i\n", temp, l4_fpage_size(l4_fpage(temp, 21, L4_FPAGE_RWX)));
		#endif

		l4_task_unmap(L4Re::Env::env()->task().cap(), l4_fpage(temp, 21, L4_FPAGE_RWX), L4_FP_OTHER_SPACES);
		temp += 1024 * 2048;
	}
}

// Forward to the default implementation.
int VFB::dispatch(l4_umword_t obj, L4::Ipc_iostream &ios)
{
	#if debug
	printf("Dataspace_svr dispatching.\n");
	#endif

	return L4Re::Util::Dataspace_svr::dispatch(obj, ios);
};

VFB_fb_svr::VFB_fb_svr(L4Re::Framebuffer::Info info, L4::Cap<L4Re::Dataspace> vfb)
{
	#if debug
	printf("Creating new VFB_fb_svr.\n");
	#endif

	// Set up information for Framebuffer_svr.
	_fb_ds = vfb;
	_info = info;
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
