/*
 * ebounce/ebounce.c
 * elilo launcher
 *
 * Switches to text mode, then launches e.efi or elilo.efi from the same
 * directory.
 *
 * Copyright (c) 2006 Christoph Pfisterer
 * All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions are
 * met:
 *
 *  * Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 *
 *  * Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the
 *    distribution.
 *
 *  * Neither the name of Christoph Pfisterer nor the names of the
 *    contributors may be used to endorse or promote products derived
 *    from this software without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
 * "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
 * LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR
 * A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT
 * OWNER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
 * SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT
 * LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
 * DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
 * THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 * OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#include "efi.h"
#include "efilib.h"
#include "efiConsoleControl.h"

EFI_GUID gEfiConsoleControlProtocolGuid = EFI_CONSOLE_CONTROL_PROTOCOL_GUID;

CHAR16 *FileNames[] = {
    L"elilo.efi",
    L"e.efi",
    NULL
};


EFI_STATUS
EFIAPI
EBounceMain (IN EFI_HANDLE           ImageHandle,
             IN EFI_SYSTEM_TABLE     *SystemTable)
{
    EFI_STATUS              Status;
    EFI_CONSOLE_CONTROL_PROTOCOL *ConsoleControl;
    EFI_CONSOLE_CONTROL_SCREEN_MODE currentMode;
    EFI_LOADED_IMAGE        *SelfLoadedImage;
    EFI_FILE                *RootDir;
    EFI_FILE                *BootFile;
    EFI_DEVICE_PATH         *DevicePath;
    CHAR16                  *DevicePathAsString;
    CHAR16                  DirName[256];
    CHAR16                  FileName[256];
    UINTN                   i, FileNameIndex;
    EFI_HANDLE              LoaderHandle;
    
    InitializeLib(ImageHandle, SystemTable);
    
    // switch to text mode
    if (BS->LocateProtocol(&gEfiConsoleControlProtocolGuid, NULL, &ConsoleControl) == EFI_SUCCESS) {
        ConsoleControl->GetMode(ConsoleControl, &currentMode, NULL, NULL);
        if (currentMode == EfiConsoleControlScreenGraphics)
            ConsoleControl->SetMode(ConsoleControl, EfiConsoleControlScreenText);
    }
    
    /// load elilo.efi or e.efi from the same directory
    
    // get loaded image protocol for ourselves
    if (BS->HandleProtocol(ImageHandle, &LoadedImageProtocol, (VOID*)&SelfLoadedImage) != EFI_SUCCESS) {
        Print(L"Can not retrieve a LoadedImageProtocol handle for ImageHandle\n");
        return EFI_NOT_FOUND;
    }
    
    // open volume
    RootDir = LibOpenRoot(SelfLoadedImage->DeviceHandle);
    if (RootDir == NULL) {
        Print(L"Can't open volume.\n");
        return EFI_NOT_FOUND;
    }
    
    // find the current directory
    DevicePathAsString = DevicePathToStr(SelfLoadedImage->FilePath);
    if (DevicePathAsString != NULL) {
        StrCpy(DirName, DevicePathAsString);
        FreePool(DevicePathAsString);
        for (i = StrLen(DirName) - 1; i > 0 && DirName[i] != '\\'; i--) ;
        DirName[i++] = '\\';
        DirName[i] = 0;
    } else {
        StrCpy(DirName, L"\\");
    }
    
    for (FileNameIndex = 0; FileNames[FileNameIndex]; FileNameIndex++) {
        // build full absolute path name
        StrCpy(FileName, DirName);
        StrCat(FileName, FileNames[FileNameIndex]);
        
        // check for presence of the file
        if (RootDir->Open(RootDir, &BootFile, FileName, EFI_FILE_MODE_READ, 0) != EFI_SUCCESS)
            continue;
        BootFile->Close(BootFile);
        
        // make a full device path for the image file
        DevicePath = FileDevicePath(SelfLoadedImage->DeviceHandle, FileName);
        
        // load the image into memory
        Status = BS->LoadImage(FALSE, ImageHandle, DevicePath, NULL, 0, &LoaderHandle);
        FreePool(DevicePath);
        if (EFI_ERROR(Status)) {
            Print(L"Can not load the file %s\n", FileName);
            return Status;
        }
        
        // start it!
        BS->StartImage(LoaderHandle, NULL, NULL);
        // just in case we get control back...
        break;
    }
    
    return EFI_SUCCESS;
}
