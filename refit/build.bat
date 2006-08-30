@echo off
IF ()==(%SDK_BUILD_ENV%) (
  echo Configuring EFI build environment...
  set SDK_BUILD_ENV=bios32
  set SDK_INSTALL_DIR=%CD%\..
  set EFI_APPLICATION_COMPATIBILITY=EFI_APP_110
  set EFI_DEBUG=NO
  set PATH=%CD%\..\bin;%PATH%
)
nmake -C -S -f refit.mak -nologo %1
