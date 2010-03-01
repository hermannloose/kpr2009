#include <l4/fb_muxer/fb_muxer.h>

int FBMuxer::dispatch(l4_umword_t obj, L4::Ipc_iostream ios)
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

			return L4_EOK;

		// Switch to an existing virtual framebuffer for display.
		case Switch:

			return L4_EOK;

		default:

			return -L4_ENOSYS;
	}
}

int main(int argc, char **argv)
{
	return 0;
}
