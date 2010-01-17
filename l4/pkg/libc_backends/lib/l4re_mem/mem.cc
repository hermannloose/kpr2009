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


const unsigned EXTENSION_SIZE = 32768; // 32k 

const unsigned char EMPTY = 1;

typedef struct mem_list_entry
{
	mem_list_entry *next;
	unsigned size;
	unsigned char flags;
} mle;

mle list_head = {&list_head, 0, 0};

void *malloc(unsigned size) throw()
{
	//enter_kdebug("malloc");
	printf("=== malloc: %i bytes requested. ===\n", size);
	mle *next = &list_head;
	mle *prev = next;
	do
	{
		//printf("Entry %08p [%08p -> %08p]\n next %08p\n size %i bytes\n flag %08x\n", next, (next + 1), ((char*) (next + 1) + next->size - 1), next->next, next->size, next->flags);
		if ((next->flags & EMPTY) && (next->size >= size))
		{
			next->flags = next->flags & ~EMPTY;
			if ((next->size - size) > sizeof(mle))
			{
				// split chunk
				//printf("Splitting chunk.\n");
				char *aux = (char*) (next + 1);
				aux += size;
				mle *newly_split = (mle*) aux;
				newly_split->next = next->next;
				newly_split->size = next->size - size - sizeof(mle);
				newly_split->flags = EMPTY;
				//printf("Split %08p [%08p -> %08p]\n next %08p\n size %i bytes\n flag %08x\n", newly_split, (newly_split + 1), ((char*) (newly_split + 1) + newly_split->size - 1), newly_split->next, newly_split->size, newly_split->flags);
				next->next = newly_split;
				next->size = size;
			}
			//printf("(old) %08p [%08p -> %08p]\n next %08p\n size %i bytes\n flag %08x\n", next, (next + 1), ((char*) (next + 1) + next->size - 1), next->next, next->size, next->flags);
			// if not, just return the whole thing
			//printf("Returning [%08x - %08x].\n", (next + 1), (((char*) next->next) - 1));
			return (void*) (next + 1);
		}
		else
		{
			prev = next;
			next = next->next;
		}
	}
	while (next != &list_head);
	//printf("Reached list_head, allocating new dataspace ...\n");
	// no suitable chunk found
	L4::Cap<L4Re::Dataspace> ds = L4Re::Util::cap_alloc.alloc<L4Re::Dataspace>();
	if (!ds.is_valid()) return 0;
	//printf("Got a dataspace cap.\n");
	long err = L4Re::Env::env()->mem_alloc()->alloc(EXTENSION_SIZE, ds);
	if (err) return 0;
	//printf("Allocated %i bytes in the dataspace.\n", EXTENSION_SIZE);
	l4_addr_t addr = 0;
	err = L4Re::Env::env()->rm()->attach(&addr, EXTENSION_SIZE, L4Re::Rm::Search_addr, ds, 0);
	if (err) return 0;
	//printf("Attached new dataspace at %x in region map.\n", addr);
	prev->next = (mle*) addr;
	//printf("Set successor of chunk %08x to %08x.\n", prev, prev->next);
	mle *new_next = &list_head;
	if ((EXTENSION_SIZE - size) > sizeof(mle))
	{
		//printf("Splitting the new memory.\n");
		char *aux = (char*) (((mle*) addr) + 1);
		aux += size;
		new_next = (mle*) aux;
		new_next->next = &list_head;
		new_next->size = EXTENSION_SIZE - 2 * sizeof(mle) - size;
		new_next->flags = EMPTY;
		//printf("Free chunk: %08x -> %08x [%i bytes] %08x.\n", new_next, new_next->next, new_next->size, new_next->flags);
	}
	mle *ret = prev->next;
	ret->next = new_next;
	ret->size = size;
	ret->flags = 0;
	//printf("Returning %08p [%08p -> %08p]\n next %08p\n size %i bytes\n flag %08x\n", ret, (ret->next + 1), ((char*) (ret + 1) + ret->size - 1), ret->next, ret->size, ret->flags);
	//printf("Returning [%08x - %08x].\n", (ret + 1), (((char*) ret->next) - 1));
	printf("=== malloc: returning %08p. ===\n", (void*) (ret + 1));
	return (void*) (ret + 1);
	/*
	void *data = 0;
	enter_kdebug("malloc");
	return (void*)data;
	*/
}


void free(void *p) throw()
{
	//printf("Free: %08p\n", p);
	//enter_kdebug("Entering free.");
	// FIXME free() does nothing, for the purpose of tracking down the "Operation not permitted" bug.
	// just mark chunk as free, without any compaction yet
	//mle *to_free = (mle*) p - 1;
	//to_free->flags = to_free->flags | EMPTY;
	//enter_kdebug("Exiting free.");
}
