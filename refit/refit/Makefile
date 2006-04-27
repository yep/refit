#
# refit/Makefile
# Build control file for the rEFIt boot menu
#

EFIINC	   = /usr/include/efi
GNUEFILIB  = /usr/lib
EFILIB	   = /usr/lib
EFICRT0	   = /usr/lib

ARCH	   = $(shell uname -m | sed s,i[3456789]86,ia32,)
INCDIR	   = -I. -I../include -I$(EFIINC) -I$(EFIINC)/$(ARCH) -I$(EFIINC)/protocol
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


CRTOBJS       = $(EFICRT0)/crt0-efi-$(ARCH).o
LDSCRIPT      = $(EFICRT0)/elf_$(ARCH)_efi.lds

LDFLAGS	     += -T $(LDSCRIPT) -shared -Bsymbolic -L$(EFILIB) -L$(GNUEFILIB) $(CRTOBJS)
LOADLIBES     = -lefi -lgnuefi $(shell $(CC) -print-libgcc-file-name)
FORMAT        = efi-app-$(ARCH)

FILES = main.o config.o menu.o screen.o icns.o image.o eei.o lib.o

TARGET = refit.efi

all: $(TARGET)

%.o: %.c 
	$(CC) $(INCDIR) $(CPPFLAGS) $(CFLAGS) -c $< -o $@

refit.so: $(FILES)
	$(LD) $(LDFLAGS) $^ -o $@ $(LOADLIBES)

refit.efi: refit.so
	$(OBJCOPY) -j .text -j .sdata -j .data -j .dynamic -j .dynsym -j .rel \
		   -j .rela -j .reloc --target=$(FORMAT) $< $@

clean:
	rm -f $(TARGET) *~ *.so $(FILES)
