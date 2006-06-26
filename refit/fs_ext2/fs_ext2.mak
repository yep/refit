#
# fs_ext2/fs_ext2.mak
# Build control file for the ext2 file system driver
#

#
# Include sdk.env environment
#

!include $(SDK_INSTALL_DIR)\build\$(SDK_BUILD_ENV)\sdk.env

#
# Set the base output name and entry point
#

BASE_NAME         = fs_ext2
IMAGE_ENTRY_POINT = Ext2EntryPoint

#
# Globals needed by master.mak
#

TARGET_BS_DRIVER = $(BASE_NAME)
SOURCE_DIR       = $(SDK_INSTALL_DIR)\refit\$(BASE_NAME)
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
    $(BUILD_DIR)\fs_ext2.obj \
    $(BUILD_DIR)\super.obj  \
    $(BUILD_DIR)\inode.obj  \
    $(BUILD_DIR)\file.obj  \
    $(BUILD_DIR)\dir.obj  \

#
# Source file dependencies
#

$(BUILD_DIR)\fs_ext2.obj    : $(*B).c $(INC_DEPS) fs_ext2.h
$(BUILD_DIR)\super.obj      : $(*B).c $(INC_DEPS) fs_ext2.h
$(BUILD_DIR)\inode.obj      : $(*B).c $(INC_DEPS) fs_ext2.h
$(BUILD_DIR)\file.obj       : $(*B).c $(INC_DEPS) fs_ext2.h
$(BUILD_DIR)\dir.obj        : $(*B).c $(INC_DEPS) fs_ext2.h

#
# Handoff to master.mak
#

!include $(SDK_INSTALL_DIR)\build\master.mak
