#
# dbounce/dbounce.mak
# Build control file for the driver loader
# 

#
# Include sdk.env environment
#

!include $(SDK_INSTALL_DIR)\build\$(SDK_BUILD_ENV)\sdk.env

#
# Set the base output name and entry point
#

BASE_NAME         = dbounce
IMAGE_ENTRY_POINT = DBounceMain

#
# Globals needed by master.mak
#

TARGET_APP = $(BASE_NAME)
SOURCE_DIR = $(SDK_INSTALL_DIR)\refit\$(BASE_NAME)
BUILD_DIR  = $(SDK_BUILD_DIR)\refit\$(BASE_NAME)

#
# Include paths
#

!include $(SDK_INSTALL_DIR)\include\$(EFI_INC_DIR)\makefile.hdr
INC = -I $(SDK_INSTALL_DIR)\include\$(EFI_INC_DIR) \
      -I $(SDK_INSTALL_DIR)\include\$(EFI_INC_DIR)\$(PROCESSOR) \
      -I $(SDK_INSTALL_DIR)\refit\include $(INC)

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
    $(BUILD_DIR)\$(BASE_NAME).obj \

#
# Source file dependencies
#

$(BUILD_DIR)\$(BASE_NAME).obj : $(*B).c $(INC_DEPS)

#
# Handoff to master.mak
#

!include $(SDK_INSTALL_DIR)\build\master.mak
