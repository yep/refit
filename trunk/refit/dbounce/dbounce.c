/*
 * dbounce/dbounce.c
 * Driver loader
 *
 * Loads all drivers from \efi\tools\drivers, then launches
 * \efi\refit\refit.efi. Both paths are searched for on all
 * available volumes if necessary.
 *
 * Copyright (c) 2006-2007 Christoph Pfisterer
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


// Paths to search for. You can override these from the compiler command
// line to build dbounce for a different loader.
#ifndef DRIVER_DIR
#define DRIVER_DIR L"\\efi\\tools\\drivers"
#endif
#ifndef CHAINLOADER
#define CHAINLOADER L"\\efi\\refit\\refit.efi"
#endif
#ifndef FALLBACKSHELL
#define FALLBACKSHELL L"\\efi\\tools\\shell.efi"
#endif


EFI_GUID gEfiConsoleControlProtocolGuid = EFI_CONSOLE_CONTROL_PROTOCOL_GUID;

EFI_HANDLE              SelfImageHandle;
EFI_LOADED_IMAGE        *SelfLoadedImage;


//
// Console stuff
//

static BOOLEAN CheckError(IN EFI_STATUS Status, IN CHAR16 *where)
{
    CHAR16 ErrorName[64];
    
    if (!EFI_ERROR(Status))
        return FALSE;
    
    StatusToString(ErrorName, Status);
    //ST->ConOut->SetAttribute(ST->ConOut, ATTR_ERROR);
    Print(L"Error: %s %s\n", ErrorName, where);
    //ST->ConOut->SetAttribute(ST->ConOut, ATTR_BASIC);
    
    return TRUE;
}

static EFI_STATUS WaitForKeyOrReset(VOID)
{
    EFI_STATUS          Status;
    EFI_INPUT_KEY       key;
    UINTN               index;
    
    for(;;) {
        Status = ST->ConIn->ReadKeyStroke(ST->ConIn, &key);
        if (Status == EFI_NOT_READY)
            BS->WaitForEvent(1, &ST->ConIn->WaitForKey, &index);
        else
            break;
    }
    if (!EFI_ERROR(Status)) {
        if (key.ScanCode == SCAN_ESC)
            RT->ResetSystem(EfiResetCold, EFI_SUCCESS, 0, NULL);
    }
    
    return Status;
}

//
// directory iteration
//

typedef struct {
    EFI_STATUS          LastStatus;
    EFI_FILE            *DirHandle;
    BOOLEAN             CloseDirHandle;
    EFI_FILE_INFO       *LastFileInfo;
} REFIT_DIR_ITER;

EFI_STATUS DirNextEntry(IN EFI_FILE *Directory, IN OUT EFI_FILE_INFO **DirEntry, IN UINTN FilterMode)
{
    EFI_STATUS Status;
    VOID *Buffer;
    UINTN LastBufferSize, BufferSize;
    INTN IterCount;
    
    for (;;) {
        
        // free pointer from last call
        if (*DirEntry != NULL) {
            FreePool(*DirEntry);
            *DirEntry = NULL;
        }
        
        // read next directory entry
        LastBufferSize = BufferSize = 256;
        Buffer = AllocatePool(BufferSize);
        for (IterCount = 0; ; IterCount++) {
            Status = Directory->Read(Directory, &BufferSize, Buffer);
            if (Status != EFI_BUFFER_TOO_SMALL || IterCount >= 4)
                break;
            if (BufferSize <= LastBufferSize) {
                Print(L"FS Driver requests bad buffer size %d (was %d), using %d instead\n", BufferSize, LastBufferSize, LastBufferSize * 2);
                BufferSize = LastBufferSize * 2;
            }
            Buffer = ReallocatePool(Buffer, LastBufferSize, BufferSize);
            LastBufferSize = BufferSize;
        }
        if (EFI_ERROR(Status)) {
            FreePool(Buffer);
            break;
        }
        
        // check for end of listing
        if (BufferSize == 0) {    // end of directory listing
            FreePool(Buffer);
            break;
        }
        
        // entry is ready to be returned
        *DirEntry = (EFI_FILE_INFO *)Buffer;
        
        // filter results
        if (FilterMode == 1) {   // only return directories
            if (((*DirEntry)->Attribute & EFI_FILE_DIRECTORY))
                break;
        } else if (FilterMode == 2) {   // only return files
            if (((*DirEntry)->Attribute & EFI_FILE_DIRECTORY) == 0)
                break;
        } else                   // no filter or unknown filter -> return everything
            break;
        
    }
    return Status;
}

VOID DirIterOpen(IN EFI_FILE *BaseDir, IN CHAR16 *RelativePath OPTIONAL, OUT REFIT_DIR_ITER *DirIter)
{
    if (RelativePath == NULL) {
        DirIter->LastStatus = EFI_SUCCESS;
        DirIter->DirHandle = BaseDir;
        DirIter->CloseDirHandle = FALSE;
    } else {
        DirIter->LastStatus = BaseDir->Open(BaseDir, &(DirIter->DirHandle), RelativePath, EFI_FILE_MODE_READ, 0);
        DirIter->CloseDirHandle = EFI_ERROR(DirIter->LastStatus) ? FALSE : TRUE;
    }
    DirIter->LastFileInfo = NULL;
}

BOOLEAN DirIterNext(IN OUT REFIT_DIR_ITER *DirIter, IN UINTN FilterMode, IN CHAR16 *FilePattern OPTIONAL,
                    OUT EFI_FILE_INFO **DirEntry)
{
    if (DirIter->LastFileInfo != NULL) {
        FreePool(DirIter->LastFileInfo);
        DirIter->LastFileInfo = NULL;
    }
    
    if (EFI_ERROR(DirIter->LastStatus))
        return FALSE;   // stop iteration
    
    for (;;) {
        DirIter->LastStatus = DirNextEntry(DirIter->DirHandle, &(DirIter->LastFileInfo), FilterMode);
        if (EFI_ERROR(DirIter->LastStatus))
            return FALSE;
        if (DirIter->LastFileInfo == NULL)  // end of listing
            return FALSE;
        if (FilePattern != NULL) {
            if ((DirIter->LastFileInfo->Attribute & EFI_FILE_DIRECTORY))
                break;
            if (MetaiMatch(DirIter->LastFileInfo->FileName, FilePattern))
                break;
            // else continue loop
        } else
            break;
    }
    
    *DirEntry = DirIter->LastFileInfo;
    return TRUE;
}

EFI_STATUS DirIterClose(IN OUT REFIT_DIR_ITER *DirIter)
{
    if (DirIter->LastFileInfo != NULL) {
        FreePool(DirIter->LastFileInfo);
        DirIter->LastFileInfo = NULL;
    }
    if (DirIter->CloseDirHandle)
        DirIter->DirHandle->Close(DirIter->DirHandle);
    return DirIter->LastStatus;
}

//
// file name manipulation
//

CHAR16 * Basename(IN CHAR16 *Path)
{
    CHAR16  *FileName;
    UINTN   i;
    
    FileName = Path;
    
    if (Path != NULL) {
        for (i = StrLen(Path); i > 0; i--) {
            if (Path[i-1] == '\\' || Path[i-1] == '/') {
                FileName = Path + i;
                break;
            }
        }
    }
    
    return FileName;
}

//
// run an EFI binary
//

static EFI_STATUS RunImage(IN EFI_HANDLE DeviceHandle, IN CHAR16 *FilePath)
{
    EFI_STATUS              Status;
    EFI_DEVICE_PATH         *DevicePath;
    EFI_HANDLE              LoaderHandle;
    CHAR16                  *FileName;
    
    FileName = Basename(FilePath);
    Print(L"Loading %s...\n", FileName);
    
    // make a full device path for the image file
    DevicePath = FileDevicePath(DeviceHandle, FilePath);
    
    // load the image into memory
    Status = BS->LoadImage(FALSE, SelfImageHandle, DevicePath, NULL, 0, &LoaderHandle);
    FreePool(DevicePath);
    if (EFI_ERROR(Status))
        return Status;
    
    // start it!
    BS->StartImage(LoaderHandle, NULL, NULL);
    
    return EFI_SUCCESS;
}

//
// driver loading
//

static EFI_STATUS LoadAllDrivers(IN EFI_HANDLE DeviceHandle, IN EFI_FILE *RootDir, IN CHAR16 *DirPath)
{
    EFI_STATUS              Status;
    REFIT_DIR_ITER          DirIter;
    EFI_FILE_INFO           *DirEntry;
    CHAR16                  FilePath[256];
    CHAR16                  ErrorMsg[256];
    
    // look through contents of the directory
    DirIterOpen(RootDir, DirPath, &DirIter);
    while (DirIterNext(&DirIter, 2, L"*.EFI", &DirEntry)) {
        if (DirEntry->FileName[0] == '.')
            continue;   // skip this
        
        SPrint(FilePath, 255, L"%s\\%s", DirPath, DirEntry->FileName);
        Status = RunImage(DeviceHandle, FilePath);
        SPrint(ErrorMsg, 255, L"while loading %s", DirEntry->FileName);
        CheckError(Status, ErrorMsg);
    }
    Status = DirIterClose(&DirIter);
    if (Status != EFI_NOT_FOUND) {
        SPrint(ErrorMsg, 255, L"while scanning for drivers");
        CheckError(Status, ErrorMsg);
        return Status;
    }
    
    return EFI_SUCCESS;
}

static EFI_STATUS ConnectAllDriversToAllControllers(VOID)
{
    EFI_STATUS  Status;
    UINTN       AllHandleCount;
    EFI_HANDLE  *AllHandleBuffer;
    UINTN       Index;
    UINTN       HandleCount;
    EFI_HANDLE  *HandleBuffer;
    UINT32      *HandleType;
    UINTN       HandleIndex;
    BOOLEAN     Parent;
    BOOLEAN     Device;
    
    Status = LibLocateHandle(AllHandles,
                             NULL,
                             NULL,
                             &AllHandleCount,
                             &AllHandleBuffer);
    if (EFI_ERROR(Status))
        return Status;
    
    for (Index = 0; Index < AllHandleCount; Index++) {
        //
        // Scan the handle database
        //
        Status = LibScanHandleDatabase(NULL,
                                       NULL,
                                       AllHandleBuffer[Index],
                                       NULL,
                                       &HandleCount,
                                       &HandleBuffer,
                                       &HandleType);
        if (EFI_ERROR (Status))
            goto Done;
        
        Device = TRUE;
        if (HandleType[Index] & EFI_HANDLE_TYPE_DRIVER_BINDING_HANDLE)
            Device = FALSE;
        if (HandleType[Index] & EFI_HANDLE_TYPE_IMAGE_HANDLE)
            Device = FALSE;
        
        if (Device) {
            Parent = FALSE;
            for (HandleIndex = 0; HandleIndex < HandleCount; HandleIndex++) {
                if (HandleType[HandleIndex] & EFI_HANDLE_TYPE_PARENT_HANDLE)
                    Parent = TRUE;
            }
            
            if (!Parent) {
                if (HandleType[Index] & EFI_HANDLE_TYPE_DEVICE_HANDLE) {
                    Status = BS->ConnectController(AllHandleBuffer[Index],
                                                   NULL,
                                                   NULL,
                                                   TRUE);
                }
            }
        }
        
        FreePool (HandleBuffer);
        FreePool (HandleType);
    }
    
Done:
    FreePool (AllHandleBuffer);
    return Status;
}

//
// volume searching
//

static EFI_STATUS CheckVolumeForPath(IN EFI_HANDLE VolumeDeviceHandle,
                                     IN CHAR16 *Path,
                                     OUT EFI_FILE **RootDirOut)
{
    EFI_STATUS  Status;
    EFI_FILE    *RootDir;
    EFI_FILE    *TestFile;
    
    // open volume
    RootDir = LibOpenRoot(VolumeDeviceHandle);
    if (RootDir == NULL) {
        Print(L"Can't open volume.\n");
        return EFI_NOT_FOUND;
    }
    
    // try to open path
    Status = RootDir->Open(RootDir, &TestFile, Path, EFI_FILE_MODE_READ, 0);
    if (EFI_ERROR(Status)) {
        RootDir->Close(RootDir);
        return Status;
    }
    TestFile->Close(TestFile);
    
    *RootDirOut = RootDir;
    return EFI_SUCCESS;
}

static EFI_STATUS FindVolumeWithPath(IN CHAR16 *Path,
                                     OUT EFI_HANDLE *DeviceHandle,
                                     OUT EFI_FILE **RootDir)
{
    EFI_STATUS              Status;
    UINTN                   HandleCount = 0;
    UINTN                   HandleIndex;
    EFI_HANDLE              *Handles;
    
    // try our volume first
    Status = CheckVolumeForPath(SelfLoadedImage->DeviceHandle, Path, RootDir);
    if (!EFI_ERROR(Status)) {
        *DeviceHandle = SelfLoadedImage->DeviceHandle;
        return Status;
    }
    
    // iterate over all volumes
    Status = LibLocateHandle(ByProtocol, &FileSystemProtocol, NULL, &HandleCount, &Handles);
    if (CheckError(Status, L"while listing all file systems"))
        return Status;
    for (HandleIndex = 0; HandleIndex < HandleCount; HandleIndex++) {
        
        // try this device
        Status = CheckVolumeForPath(Handles[HandleIndex], Path, RootDir);
        if (!EFI_ERROR(Status)) {
            *DeviceHandle = Handles[HandleIndex];
            FreePool(Handles);
            return Status;
        }
        
    }
    FreePool(Handles);
    
    return EFI_NOT_FOUND;
}


EFI_STATUS
EFIAPI
DBounceMain (IN EFI_HANDLE           ImageHandle,
             IN EFI_SYSTEM_TABLE     *SystemTable)
{
    EFI_STATUS              Status;
    EFI_CONSOLE_CONTROL_PROTOCOL *ConsoleControl;
    EFI_CONSOLE_CONTROL_SCREEN_MODE currentMode;
    EFI_HANDLE              DeviceHandle;
    EFI_FILE                *RootDir;
    
    SelfImageHandle = ImageHandle;
    InitializeLib(ImageHandle, SystemTable);
    
    // switch to text mode
    if (BS->LocateProtocol(&gEfiConsoleControlProtocolGuid, NULL, &ConsoleControl) == EFI_SUCCESS) {
        ConsoleControl->GetMode(ConsoleControl, &currentMode, NULL, NULL);
        if (currentMode == EfiConsoleControlScreenGraphics)
            ConsoleControl->SetMode(ConsoleControl, EfiConsoleControlScreenText);
    }
    
    // get loaded image protocol for ourselves
    if (BS->HandleProtocol(SelfImageHandle, &LoadedImageProtocol, (VOID*)&SelfLoadedImage) != EFI_SUCCESS) {
        Print(L"Can not retrieve a LoadedImageProtocol handle for ImageHandle\n");
        return EFI_NOT_FOUND;
    }
    
    // load all drivers from drivers directory
    Status = FindVolumeWithPath(DRIVER_DIR, &DeviceHandle, &RootDir);
    if (EFI_ERROR(Status)) {
        Print(L"Warning: Can't find a volume containing '" DRIVER_DIR L"'.\n");
    } else {
        Status = LoadAllDrivers(DeviceHandle, RootDir, DRIVER_DIR);
        RootDir->Close(RootDir);
        
        Status = ConnectAllDriversToAllControllers();
    }
    
    // load chainloaded loader
    Status = FindVolumeWithPath(CHAINLOADER, &DeviceHandle, &RootDir);
    if (EFI_ERROR(Status)) {
        Print(L"Error: Can't find a volume containing '" CHAINLOADER L"'.\n");
    } else {
        RootDir->Close(RootDir);
        
        Status = RunImage(DeviceHandle, CHAINLOADER);
        CheckError(Status, L"while loading '" CHAINLOADER L"'.\n");
        // NOTE: The loader is not supposed to return to us if
        //  it succeeds in loading an OS.
    }
    
    // fall back to the EFI shell
    Status = FindVolumeWithPath(FALLBACKSHELL, &DeviceHandle, &RootDir);
    if (EFI_ERROR(Status)) {
        Print(L"Error: Can't find a volume containing '" FALLBACKSHELL L"'.\n");
    } else {
        RootDir->Close(RootDir);
        
        Print(L"\nPress ESC to reboot or any other key to start the EFI Shell.\n\n");
        WaitForKeyOrReset();
        
        Status = RunImage(DeviceHandle, FALLBACKSHELL);
        CheckError(Status, L"while loading '" FALLBACKSHELL L"'.\n");
    }
    
    Print(L"\nPress ESC to reboot or any other key to return control to the firmware.\n\n");
    WaitForKeyOrReset();
    
    return Status;
}
