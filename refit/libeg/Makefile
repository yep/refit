#
# libeg/Makefile
# Build control file for the libeg library
#

EFIINC	   = /usr/include/efi
GNUEFILIB  = /usr/lib
EFILIB	   = /usr/lib
EFICRT0	   = /usr/lib

ARCH	   = $(shell uname -m | sed s,i[3456789]86,ia32,)
INCDIR	   = -I. -I$(EFIINC) -I$(EFIINC)/$(ARCH) -I$(EFIINC)/protocol
CPPFLAGS   = -DCONFIG_$(ARCH) 

OPTIMFLAGS = -O2 
DEBUGFLAGS = -Wall
CFLAGS	   = $(OPTIMFLAGS) -mcpu=i386 -fpic -fshort-wchar $(DEBUGFLAGS)
LDFLAGS	   = -nostdlib -znocombreloc
INSTALL	   = install

prefix	        = /usr/bin/
CC		= $(prefix)gcc
AS		= $(prefix)as
LD		= $(prefix)ld
AR		= $(prefix)ar
RANLIB	        = $(prefix)ranlib
OBJCOPY	        = $(prefix)objcopy

ifeq ($(ARCH),ia64)
 GCC_VERSION=$(shell $(CROSS_COMPILE)$(CC) -v 2>&1 | fgrep 'gcc version' | cut -f3 -d' ' | cut -f1 -d'.')

 ifneq ($(GCC_VERSION),2)
        CFLAGS += -frename-registers
 endif

 # EFI specs allows only lower floating point partition to be used
 CFLAGS += -mfixed-range=f32-f127
endif


OBJS    = screen.o image.o load_bmp.o load_icns.o
TARGET  = libeg.a

all: $(TARGET)

%.o: %.c 
	$(CC) $(INCDIR) $(CPPFLAGS) $(CFLAGS) -c $< -o $@

libeg.a: $(OBJS)
	ar cq $@ $(OBJS)

clean:
	rm -f $(TARGET) *~ *.so $(OBJS)

# EOF
