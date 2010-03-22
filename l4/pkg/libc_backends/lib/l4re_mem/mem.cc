/**
 * \file   libc_backends/l4re_mem/mem.cc
 */
/*
 * (c) 2004-2009 Technische Universität Dresden
 * This file is part of TUD:OS and distributed under the terms of the
 * GNU Lesser General Public License 2.1.
 * Please see the COPYING-LGPL-2.1 file for details.
 */
#include <stdlib.h>
#include <stdio.h>
#include <l4/re/dataspace>
#include <l4/re/env>
#include <l4/re/util/cap_alloc>
#include <l4/sys/capability>
#include <l4/sys/kdebug.h>
#include <l4/sys/l4int.h>
#include <l4/util/atomic.h>

#include <assert.h>

#define debug 0

// FIXME Refactor this using a double-linked list

const unsigned EXTENSION_SIZE = 32768; // 32k 

const unsigned char EMPTY = 1;

const unsigned PAD_SIZE = sizeof(l4_umword_t) - ((sizeof(unsigned) + sizeof(unsigned char)) % sizeof(l4_umword_t));

typedef struct mem_list_entry
{
	mem_list_entry *next;
	unsigned size;
	unsigned char flags;
	char pad[PAD_SIZE];
} mle;

mle list_head = {&list_head, 0, 0};

// FIXME: Semaphores would be nicer, but is this possible here?

void lock(l4_umword_t *mutex)
{
	#if debug
	printf("Locking mutex.\n");
	#endif

	while(!l4util_cmpxchg(mutex, 0, 1));
	
	#if debug
	printf("Locked mutex.\n");
	#endif
}

void unlock(l4_umword_t *mutex)
{
	*mutex = 0;

	#if debug
	printf("Unlocked mutex.\n");
	#endif
}

l4_umword_t mutex = 0;

mle * split_chunk(mle *chunk, unsigned size)
{
	#if debug
	printf("Chunk to split: %p size %i flags %x next %p\n", chunk, chunk->size, chunk->flags, chunk->next, size);
	#endif

	// PRE
	assert(chunk != NULL);
	assert(chunk->next != NULL);
	assert(size > 0);
	assert(size < (chunk->size - sizeof(mle)));

	unsigned unsplit_size = chunk->size;

	char *aux = (char*) (chunk + 1);
	aux += size;

	mle *ins = (mle*) aux;
	ins->next = chunk->next;
	ins->size = chunk->size - (size + sizeof(mle));
	ins->flags = EMPTY;

	#if debug
	printf("New chunk: %p size %i flags %x next %p\n", ins, ins->size, ins->flags, ins->next);
	#endif

	chunk->next = ins;
	chunk->size = size;

	assert(((char*) ins - (char*) (chunk + 1)) == size);

	// POST
	assert(chunk != NULL);
	assert(ins != NULL);
	assert(chunk->next == ins);
	assert(ins->next != NULL);

	return ins;
}

void *malloc(unsigned size) throw()
{
	#if debug
	printf("\nlist-head: %p size: %i\n", &list_head, size);
	enter_kdebug("malloc");
	#endif

	lock(&mutex);
	
	#if debug
	printf("=== malloc: %i bytes requested. ", size);
	#endif

	if (size % sizeof(l4_umword_t)) {
		size += sizeof(l4_umword_t) - (size % sizeof(l4_umword_t));
		
		#if debug
		printf("Padding to %i bytes. ===\n", size);
		#endif		
	} else {
		#if debug
		printf("===\n");
		#endif
	}

	// Assert that memory is requested out word-aligned.
	assert(!(size % sizeof(l4_umword_t)));

	mle *next = &list_head;
	mle *prev = next;

	do {
		assert(next != NULL);

		if ((next->flags & EMPTY) && (next->size >= size)) {
			// Assert that the chunk is really large enough.
			assert(next < next->next ? (next - next->next) > size : 1);

			next->flags = next->flags & ~EMPTY;
			if ((next->size - size) > sizeof(mle)) {
				#if debug
				printf("Splitting chunk.\n");
				#endif

				mle *newly_split = split_chunk(next, size);

				#if debug
				printf("%p [%i / %x] -> %p\n", newly_split, newly_split->size, newly_split->flags, newly_split->next);
				printf("Links: %p -> %p -> %p\n", next, newly_split, newly_split->next);
				#endif
			}
			// if not, just return the whole thing
			void *ret_split = (void*) (next + 1);
			unlock(&mutex);

			#if debug
			printf("=== malloc: returning %08p. ===\n", ret_split);
			#endif

			assert(((char*) ret_split - (char*) next) == sizeof(mle));
		
			// Assert that memory is handed out word-aligned.
			assert(((l4_addr_t) ret_split % sizeof(l4_umword_t)) == 0);

			#if debug
			enter_kdebug("malloc pre-return");
			#endif

			return ret_split;

		} else {
			prev = next;
			next = next->next;
		}
	} while (next != &list_head);

	// no suitable chunk found
	L4::Cap<L4Re::Dataspace> ds = L4Re::Util::cap_alloc.alloc<L4Re::Dataspace>();
	if (!ds.is_valid()) {
		unlock(&mutex);

		return 0;
	}
	long err = L4Re::Env::env()->mem_alloc()->alloc(EXTENSION_SIZE, ds);
	if (err) {
		unlock(&mutex);

		return 0;
	}
	l4_addr_t addr = 0;
	err = L4Re::Env::env()->rm()->attach(&addr, EXTENSION_SIZE, L4Re::Rm::Search_addr, ds, 0);
	if (err) {
		unlock(&mutex);

		return 0;
	}
	prev->next = (mle*) addr;
	mle *new_next = &list_head;
	if ((EXTENSION_SIZE - size) > sizeof(mle))
	{
		char *aux = (char*) (((mle*) addr) + 1);
		aux += size;
		new_next = (mle*) aux;
		new_next->next = &list_head;
		new_next->size = EXTENSION_SIZE - 2 * sizeof(mle) - size;
		new_next->flags = EMPTY;
	}
	mle *ret = prev->next;
	ret->next = new_next;
	ret->size = size;
	ret->flags = 0;

	#if debug
	printf("=== malloc: returning %08p. ===\n", (void*) (ret + 1));
	#endif
	
	void *ret_new = (void*) (ret + 1);
	unlock(&mutex);

	assert(((char*) ret_new - (char*) ret) == sizeof(mle));

	// Assert that memory is handed out word-aligned.
	assert(((l4_addr_t) ret_new % sizeof(l4_umword_t)) == 0);

	#if debug
	enter_kdebug("malloc pre_return");
	#endif

	return ret_new;
}

void free(void *p) throw()
{
	assert(p != NULL);

	#if debug
	printf("free %08p\n", p);
	#endif

	mle *next = &list_head;
	mle *prev = next;
	mle *to_free = (mle*) p - 1;

	lock(&mutex);

	to_free->flags = to_free->flags | EMPTY;

	do {
		if (next == to_free) {
			// Compact chunks if possible.
			if (next->next->flags & EMPTY) {
				next->size += sizeof(mle) + next->next->size;
				next->next = next->next->next;
			}
			if (prev->flags & EMPTY) {
				prev->size += sizeof(mle) + next->size;
				prev->next = next->next;
			}
			break;
		} else {
			prev = next;
			next = next->next;
		}
	} while (next != &list_head);
	
	unlock(&mutex);
}
