#
# fsw/fsw_ext2.mak
# Build control file for the ext2 file system driver
#

#
# Include sdk.env environment
#

!include $(SDK_INSTALL_DIR)\build\$(SDK_BUILD_ENV)\sdk.env

#
# Set the base output name and entry point
#

BASE_NAME         = fsw_ext2
IMAGE_ENTRY_POINT = fsw_efi_main

#
# Globals needed by master.mak
#

TARGET_BS_DRIVER = $(BASE_NAME)
SOURCE_DIR       = $(SDK_INSTALL_DIR)\refit\fsw
BUILD_DIR        = $(SDK_BUILD_DIR)\refit\$(BASE_NAME)

#
# Include paths
#

!include $(SDK_INSTALL_DIR)\include\$(EFI_INC_DIR)\makefile.hdr
INC = -I $(SDK_INSTALL_DIR)\include\$(EFI_INC_DIR) \
      -I $(SDK_INSTALL_DIR)\include\$(EFI_INC_DIR)\$(PROCESSOR) $(INC)

#
# Libraries
#

LIBS = $(LIBS) $(SDK_BUILD_DIR)\lib\libefi\libefi.lib

#
# Default target
#

all : dirs $(LIBS) $(OBJECTS)
	@echo Copying $(BASE_NAME).efi to current directory
	@copy $(SDK_BIN_DIR)\$(BASE_NAME).efi .

#
# Program object files
#

OBJECTS = $(OBJECTS) \
    $(BUILD_DIR)\fsw_efi.obj \
    $(BUILD_DIR)\fsw_efi_lib.obj \
    $(BUILD_DIR)\fsw_core.obj \
    $(BUILD_DIR)\fsw_lib.obj \
    $(BUILD_DIR)\fsw_ext2.obj \

INC_DEPS = $(INC_DEPS) fsw_efi.h fsw_efi_base.h fsw_core.h fsw_ext2.h fsw_ext2_disk.h

#
# Source file dependencies
#

$(BUILD_DIR)\fsw_efi.obj    : $(*B).c $(INC_DEPS)
$(BUILD_DIR)\fsw_efi_lib.obj : $(*B).c $(INC_DEPS)
$(BUILD_DIR)\fsw_core.obj   : $(*B).c $(INC_DEPS)
$(BUILD_DIR)\fsw_lib.obj    : $(*B).c $(INC_DEPS)
$(BUILD_DIR)\fsw_ext2.obj   : $(*B).c $(INC_DEPS)

#
# Handoff to master.mak
#

!include $(SDK_INSTALL_DIR)\build\master.mak
