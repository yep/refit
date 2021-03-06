#
# fsw/fsw_reiserfs.mak
# Build control file for the reiserfs file system driver
#

#
# Include sdk.env environment
#

!include $(SDK_INSTALL_DIR)\build\$(SDK_BUILD_ENV)\sdk.env

#
# Set the base output name and entry point
#

BASE_NAME         = fsw_reiserfs
IMAGE_ENTRY_POINT = fsw_efi_main

#
# Globals needed by master.mak
#

TARGET_BS_DRIVER = $(BASE_NAME)
SOURCE_DIR       = $(SDK_INSTALL_DIR)\refit\fsw
BUILD_DIR        = $(SDK_BUILD_DIR)\refit\$(BASE_NAME)

C_FLAGS = $(C_FLAGS) /D HOST_EFI /D FSTYPE=reiserfs

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
	@copy $(SDK_BIN_DIR)\$(BASE_NAME).efi $(BASE_NAME)_$(SDK_BUILD_ENV).efi

#
# Program object files
#

OBJECTS = $(OBJECTS) \
    $(BUILD_DIR)\fsw_efi.obj \
    $(BUILD_DIR)\fsw_efi_lib.obj \
    $(BUILD_DIR)\fsw_core.obj \
    $(BUILD_DIR)\fsw_lib.obj \
    $(BUILD_DIR)\fsw_reiserfs.obj \

INC_DEPS = $(INC_DEPS) fsw_base.h fsw_efi_base.h fsw_core.h fsw_efi.h fsw_reiserfs.h fsw_reiserfs_disk.h

#
# Source file dependencies
#

$(BUILD_DIR)\fsw_efi.obj    : $(*B).c $(INC_DEPS)
$(BUILD_DIR)\fsw_efi_lib.obj : $(*B).c $(INC_DEPS)
$(BUILD_DIR)\fsw_core.obj   : $(*B).c $(INC_DEPS)
$(BUILD_DIR)\fsw_lib.obj    : $(*B).c $(INC_DEPS) fsw_strfunc.h
$(BUILD_DIR)\fsw_reiserfs.obj : $(*B).c $(INC_DEPS)

#
# Handoff to master.mak
#

!include $(SDK_INSTALL_DIR)\build\master.mak
