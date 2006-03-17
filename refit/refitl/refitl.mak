#
# refitl/refitl.mak
# Build control file for the rEFIt boot menu (text-only version)
#

#
# Include sdk.env environment
#

!include $(SDK_INSTALL_DIR)\build\$(SDK_BUILD_ENV)\sdk.env

#
# Set the base output name and entry point
#

BASE_NAME         = refitl
REAL_BASE_NAME    = refit
IMAGE_ENTRY_POINT = RefitMain

#
# Globals needed by master.mak
#

TARGET_APP = $(BASE_NAME)
SOURCE_DIR = $(SDK_INSTALL_DIR)\refit\$(REAL_BASE_NAME)
BUILD_DIR  = $(SDK_BUILD_DIR)\refit\$(BASE_NAME)

C_FLAGS = $(C_FLAGS) /D TEXTONLY

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

#
# Program object files
#

OBJECTS = $(OBJECTS) \
    $(BUILD_DIR)\main.obj \
    $(BUILD_DIR)\menu.obj \
    $(BUILD_DIR)\lib.obj  \

#
# Source file dependencies
#

$(BUILD_DIR)\main.obj : $(SOURCE_DIR)\$(*B).c $(INC_DEPS)
$(BUILD_DIR)\menu.obj : $(SOURCE_DIR)\$(*B).c $(INC_DEPS)
$(BUILD_DIR)\lib.obj  : $(SOURCE_DIR)\$(*B).c $(INC_DEPS)

#
# Handoff to master.mak
#

!include $(SDK_INSTALL_DIR)\build\master.mak
