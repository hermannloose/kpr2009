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

l4_umword_t mutex = 0;

void lock(l4_umword_t *mutex)
{
	while(!l4util_cmpxchg(mutex, 0, 1));
}

void unlock(l4_umword_t *mutex)
{
	*mutex = 0;
}

void *malloc(unsigned size) throw()
{
	//enter_kdebug("malloc");
	lock(&mutex);
	printf("=== malloc: %i bytes requested. ", size);
	size += sizeof(l4_umword_t) - (size % sizeof(l4_umword_t));
	printf("Padding to %i bytes. ===\n", size);
	mle *next = &list_head;
	mle *prev = next;
	
	do
	{
		if ((next->flags & EMPTY) && (next->size >= size))
		{
			next->flags = next->flags & ~EMPTY;
			if ((next->size - size) > sizeof(mle))
			{
				// split chunk
				char *aux = (char*) (next + 1);
				aux += size;
				mle *newly_split = (mle*) aux;
				newly_split->next = next->next;
				newly_split->size = next->size - size - sizeof(mle);
				newly_split->flags = EMPTY;
				next->next = newly_split;
				next->size = size;
			}
			// if not, just return the whole thing
			void *ret_split = (void*) (next + 1);
			unlock(&mutex);
			return ret_split;
		}
		else
		{
			prev = next;
			next = next->next;
		}
	}
	while (next != &list_head);
	
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
	printf("=== malloc: returning %08p. ===\n", (void*) (ret + 1));
	void *ret_new = (void*) (ret + 1);
	unlock(&mutex);
	return ret_new;
}


void free(void *p) throw()
{
	lock(&mutex);
	printf("Free: %08p\n", p);
	//enter_kdebug("Entering free.");
	// FIXME free() does nothing, for the purpose of tracking down the "Operation not permitted" bug.
	// just mark chunk as free, without any compaction yet
	//mle *to_free = (mle*) p - 1;
	//to_free->flags = to_free->flags | EMPTY;
	unlock(&mutex);
	//enter_kdebug("Exiting free.");
}
