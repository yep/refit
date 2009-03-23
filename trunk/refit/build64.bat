@echo off
echo Configuring EFI build environment...
set SDK_BUILD_ENV=em64t
set SDK_INSTALL_DIR=%CD%\..
set EFI_APPLICATION_COMPATIBILITY=EFI_APP_110
set EFI_DEBUG=NO
nmake -C -S -f refit.mak -nologo %1
