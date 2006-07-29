#
# refit.mak
# Build control file for all rEFIt components
# 

SOURCE_DIR = $(SDK_INSTALL_DIR)\refit

!include $(SDK_INSTALL_DIR)\build\$(SDK_BUILD_ENV)\sdk.env

all :
	cd $(SOURCE_DIR)\libeg
	nmake -f libeg.mak all
	cd $(SOURCE_DIR)

	cd $(SOURCE_DIR)\refit
	nmake -f refit.mak all
	cd $(SOURCE_DIR)

	cd $(SOURCE_DIR)\gptsync
	nmake -f gptsync.mak all
	cd $(SOURCE_DIR)

#	cd $(SOURCE_DIR)\ebounce
#	nmake -f ebounce.mak all
#	cd $(SOURCE_DIR)

#	cd $(SOURCE_DIR)\TextMode
#	nmake -f TextMode.mak all
#	cd $(SOURCE_DIR)

	cd $(SOURCE_DIR)\dumpprot
	nmake -f dumpprot.mak all
	cd $(SOURCE_DIR)

#	cd $(SOURCE_DIR)\fs_ext2
#	nmake -f fs_ext2.mak all
#	cd $(SOURCE_DIR)

	cd $(SOURCE_DIR)\fsw
	nmake -f fsw_ext2.mak all
	cd $(SOURCE_DIR)

	cd $(SOURCE_DIR)\fsw
	nmake -f fsw_reiserfs.mak all
	cd $(SOURCE_DIR)
