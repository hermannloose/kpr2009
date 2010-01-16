PKGDIR       ?= ../..
L4DIR        ?= $(PKGDIR)/../..

TARGET	      = libc_be_mem.a libc_be_mem.so
LINK_INCR     = libc_be_mem.a
PC_FILENAME   = libc_be_mem
REQUIRES_LIBS = l4re libsupc++
SRC_CC	      = mem.cc

include $(L4DIR)/mk/lib.mk

LDFLAGS := $(filter-out -gc-sections,$(LDFLAGS))
