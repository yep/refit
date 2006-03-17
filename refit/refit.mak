#
# refit.mak
# Build control file for all rEFIt components
# 

SOURCE_DIR = $(SDK_INSTALL_DIR)\refit

!include $(SDK_INSTALL_DIR)\build\$(SDK_BUILD_ENV)\sdk.env

all :
	cd $(SOURCE_DIR)\refit
	nmake -f refit.mak all
	cd $(SOURCE_DIR)

	cd $(SOURCE_DIR)\refit
	nmake -f refitl.mak all
	cd $(SOURCE_DIR)

	cd $(SOURCE_DIR)\ebounce
	nmake -f ebounce.mak all
	cd $(SOURCE_DIR)

	cd $(SOURCE_DIR)\TextMode
	nmake -f TextMode.mak all
	cd $(SOURCE_DIR)

!IF "$(PROCESSOR)" == "Ia64"


!ENDIF

