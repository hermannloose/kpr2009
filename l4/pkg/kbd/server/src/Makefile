PKGDIR	?= ../..
L4DIR		?= $(PKGDIR)/../..

TARGET	= $(PKGNAME)

L4_MULTITHREADED = y

# list your .c or .cc files here
SRC_C		=
SRC_CC	= main.cc

# list requirements of your program here
REQUIRES_LIBS  += libstdc++ libsigma0 libio-direct

include $(L4DIR)/mk/prog.mk
