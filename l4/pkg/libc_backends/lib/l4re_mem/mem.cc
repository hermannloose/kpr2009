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

typedef struct mem_list_entry
{
	mem_list_entry *next;
	mem_list_entry *prev;
	unsigned int size;
	unsigned char flags;
} mle;

mle list_head = {&list_head, &list_head, 0, 0};

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
	printf("Chunk to split: %p size %i flags %x next %p prev %p\n", chunk, chunk->size, chunk->flags, chunk->next, chunk->prev);
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
	chunk->next->prev = ins;
	ins->prev = chunk;
	ins->size = chunk->size - (size + sizeof(mle));
	ins->flags = EMPTY;

	#if debug
	printf("New chunk: %p size %i flags %x next %p prev %p\n", ins, ins->size, ins->flags, ins->next, ins->prev);
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
	l4_msgtag_t err;

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

	// Assert that memory is requested word-aligned.
	assert(!(size % sizeof(l4_umword_t)));

	mle *iter = &list_head;

	do {
		assert(iter != NULL);

		if ((iter->flags & EMPTY) && (iter->size >= size)) {
			// Assert that the chunk is really large enough.
			assert(iter < iter->next ? (iter - iter->next) > size : 1);

			iter->flags = iter->flags & ~EMPTY;
			if ((iter->size - size) > sizeof(mle)) {
				#if debug
				printf("Splitting chunk.\n");
				#endif

				mle *newly_split = split_chunk(iter, size);

				#if debug
				printf("%p [%i / %x] -> %p\n", newly_split, newly_split->size, newly_split->flags, newly_split->next);
				printf("Links: %p -> %p -> %p\n", iter, newly_split, newly_split->next);
				#endif
			}
			// if not, just return the whole thing
			void *ret_split = (void*) (iter + 1);
			unlock(&mutex);

			#if debug
			printf("=== malloc: returning %08p. ===\n", ret_split);
			#endif

			assert(((char*) ret_split - (char*) iter) == sizeof(mle));
		
			// Assert that memory is handed out word-aligned.
			assert(((l4_addr_t) ret_split % sizeof(l4_umword_t)) == 0);

			#if debug
			enter_kdebug("malloc pre-return");
			#endif

			return ret_split;

		} else {
			iter = iter->next;
		}
	} while (iter != &list_head);

	// No suitable chunk found.

	// Allocate new memory to hand out.
	L4::Cap<L4Re::Dataspace> ds = L4Re::Util::cap_alloc.alloc<L4Re::Dataspace>();
	if (!ds.is_valid()) {
		printf("Could not get a valid dataspace capability!\n");
		unlock(&mutex);

		return NULL;
	}
	if (L4Re::Env::env()->mem_alloc()->alloc(EXTENSION_SIZE, ds)) {
		printf("Could not allocate a new dataspace!\n");
		unlock(&mutex);

		return NULL;
	}

	// Attach the allocated memory in the region map.
	l4_addr_t addr = 0;

	if (L4Re::Env::env()->rm()->attach(&addr, EXTENSION_SIZE, L4Re::Rm::Search_addr, ds, 0)) {
		printf("Could not attach allocated memory in region map!\n");
		unlock(&mutex);

		return NULL;
	}

	mle *allocated = (mle*) addr;
	
	allocated->next = iter;
	allocated->prev = iter->prev;
	list_head.prev = allocated;
	allocated->size = EXTENSION_SIZE - sizeof(mle);
	allocated->flags = EMPTY;

	allocated->prev->next = allocated;

	if ((allocated->size - size) > sizeof(mle)) {
		split_chunk(allocated, size);
	}

	allocated->flags = allocated->flags & ~EMPTY;

	#if debug
	printf("=== malloc: returning %08p. ===\n", (void*) (ret + 1));
	#endif
	
	void *ret_new = (void*) (allocated + 1);
	unlock(&mutex);

	assert(((char*) ret_new - (char*) allocated) == sizeof(mle));

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

	lock(&mutex);

	mle *to_free = (mle*) p - 1;
	mle *next = to_free->next;
	mle *prev = to_free->prev;

	to_free->flags |= EMPTY;

	if (next->flags & EMPTY) {
		to_free->next = next->next;
		to_free->next->prev = to_free;
		to_free->size += sizeof(mle) + next->size;
	}
	if (prev->flags & EMPTY) {
		prev->next = to_free->next;
		to_free->next->prev = prev;
		prev->size += sizeof(mle) + to_free->size;
	}

	unlock(&mutex);
}
