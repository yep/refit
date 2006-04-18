/*
 * refit/lib.c
 * General library functions
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

#include "lib.h"

// variables

EFI_HANDLE       SelfImageHandle;
EFI_LOADED_IMAGE *SelfLoadedImage;
EFI_FILE         *SelfRootDir;
EFI_FILE         *SelfDir;
CHAR16           *SelfDirPath;

REFIT_VOLUME     *SelfVolume = NULL;
REFIT_VOLUME     **Volumes = NULL;
UINTN            VolumesCount = 0;

//
// self recognition stuff
//

EFI_STATUS InitRefitLib(IN EFI_HANDLE ImageHandle)
{
    EFI_STATUS  Status;
    CHAR16      *DevicePathAsString;
    CHAR16      BaseDirectory[256];
    UINTN       i;
    
    SelfImageHandle = ImageHandle;
    Status = BS->HandleProtocol(SelfImageHandle, &LoadedImageProtocol, (VOID **) &SelfLoadedImage);
    if (CheckFatalError(Status, L"while getting a LoadedImageProtocol handle"))
        return EFI_LOAD_ERROR;
    
    /*
    if (SelfLoadedImage->LoadOptionsSize > 0) {
        CHAR16 Buffer[1024];
        UINTN Length = SelfLoadedImage->LoadOptionsSize / 2;
        if (Length > 1023)
            Length = 1023;
        CopyMem(Buffer, SelfLoadedImage->LoadOptions, SelfLoadedImage->LoadOptionsSize);
        Buffer[Length] = 0;
        Print(L"Load options: '%s'\n", Buffer);
        CheckError(EFI_LOAD_ERROR, L"FOR DEBUGGING");
    }
    */
    
    SelfRootDir = LibOpenRoot(SelfLoadedImage->DeviceHandle);
    
    // find the current directory
    DevicePathAsString = DevicePathToStr(SelfLoadedImage->FilePath);
    if (DevicePathAsString != NULL) {
        StrCpy(BaseDirectory, DevicePathAsString);
        FreePool(DevicePathAsString);
        for (i = StrLen(BaseDirectory); i > 0 && BaseDirectory[i] != '\\'; i--) ;
        BaseDirectory[i] = 0;
    } else
        BaseDirectory[0] = 0;
    SelfDirPath = StrDuplicate(BaseDirectory);
    
    Status = SelfRootDir->Open(SelfRootDir, &SelfDir, SelfDirPath, EFI_FILE_MODE_READ, 0);
    if (CheckFatalError(Status, L"while opening our installation directory"))
        return EFI_LOAD_ERROR;
    
    return EFI_SUCCESS;
}

//
// list functions
//

VOID CreateList(OUT VOID ***ListPtr, OUT UINTN *ElementCount, IN UINTN InitialElementCount)
{
    UINTN AllocateCount;
    
    *ElementCount = InitialElementCount;
    if (*ElementCount > 0) {
        AllocateCount = (*ElementCount + 7) & ~7;   // next multiple of 8
        *ListPtr = AllocatePool(sizeof(VOID *) * AllocateCount);
    } else {
        *ListPtr = NULL;
    }
}

VOID AddListElement(IN OUT VOID ***ListPtr, IN OUT UINTN *ElementCount, IN VOID *NewElement)
{
    UINTN AllocateCount;
    
    if ((*ElementCount & 7) == 0) {
        AllocateCount = *ElementCount + 8;
        if (*ElementCount == 0)
            *ListPtr = AllocatePool(sizeof(VOID *) * AllocateCount);
        else
            *ListPtr = ReallocatePool(*ListPtr, sizeof(VOID *) * (*ElementCount), sizeof(VOID *) * AllocateCount);
    }
    (*ListPtr)[*ElementCount] = NewElement;
    (*ElementCount)++;
}

VOID FreeList(IN OUT VOID ***ListPtr, IN OUT UINTN *ElementCount)
{
    UINTN i;
    
    if (*ElementCount > 0) {
        for (i = 0; i < *ElementCount; i++) {
            // TODO: call a user-provided routine for each element here
            FreePool((*ListPtr)[i]);
        }
        FreePool(*ListPtr);
    }
}

//
// volume functions
//

static VOID ScanVolume(IN OUT REFIT_VOLUME *Volume)
{
    EFI_STATUS              Status;
    EFI_DEVICE_PATH         *DevicePath, *NextDevicePath;
    EFI_DEVICE_PATH         *DiskDevicePath, *RemainingDevicePath;
    EFI_HANDLE              DiskHandle;
    EFI_BLOCK_IO            *DiskBlockIO;
    UINTN                   PartialLength;
    EFI_FILE_SYSTEM_INFO    *FileSystemInfoPtr;
    
    // get device path
    Volume->DevicePath = DevicePathFromHandle(Volume->DeviceHandle);
    //if (Volume->DevicePath != NULL) {
    //    Print(L"  * %s\n", DevicePathToStr(Volume->DevicePath));
    //    DumpHex(2, 0, DevicePathSize(Volume->DevicePath), Volume->DevicePath);
    //}
    
    // detect device type
    Volume->DiskKind = DISK_KIND_INTERNAL;  // default
    DevicePath = Volume->DevicePath;
    while (DevicePath != NULL && !IsDevicePathEndType(DevicePath)) {
        NextDevicePath = NextDevicePathNode(DevicePath);
        
        if (DevicePathType(DevicePath) == MESSAGING_DEVICE_PATH &&
            (DevicePathSubType(DevicePath) == MSG_USB_DP ||
             DevicePathSubType(DevicePath) == MSG_USB_CLASS_DP ||
             DevicePathSubType(DevicePath) == MSG_1394_DP ||
             DevicePathSubType(DevicePath) == MSG_FIBRECHANNEL_DP))
            Volume->DiskKind = DISK_KIND_EXTERNAL;    // USB/FireWire/FC device -> external
        if (DevicePathType(DevicePath) == MEDIA_DEVICE_PATH &&
            DevicePathSubType(DevicePath) == MEDIA_CDROM_DP)
            Volume->DiskKind = DISK_KIND_OPTICAL;     // El Torito entry -> optical disk
        
        if (DevicePathType(DevicePath) == MEDIA_DEVICE_PATH &&
            DevicePathSubType(DevicePath) == MEDIA_VENDOR_DP)
            Volume->IsLegacy = TRUE;                  // legacy BIOS device entry
        
        if (DevicePathType(DevicePath) == MESSAGING_DEVICE_PATH) {
            // make a device path for the whole device
            PartialLength = (UINT8 *)NextDevicePath - (UINT8 *)(Volume->DevicePath);
            DiskDevicePath = (EFI_DEVICE_PATH *)AllocatePool(PartialLength + sizeof(EFI_DEVICE_PATH));
            CopyMem(DiskDevicePath, Volume->DevicePath, PartialLength);
            CopyMem((UINT8 *)DiskDevicePath + PartialLength, EndDevicePath, sizeof(EFI_DEVICE_PATH));
            
            // get the handle for that path
            RemainingDevicePath = DiskDevicePath;
            //Print(L"  * looking at %s\n", DevicePathToStr(RemainingDevicePath));
            Status = BS->LocateDevicePath(&BlockIoProtocol, &RemainingDevicePath, &DiskHandle);
            //Print(L"  * remaining: %s\n", DevicePathToStr(RemainingDevicePath));
            FreePool(DiskDevicePath);
            
            if (!EFI_ERROR(Status)) {
                //Print(L"  - original handle: %08x - disk handle: %08x\n", (UINT32)DeviceHandle, (UINT32)DiskHandle);
                
                // look at the BlockIO protocol
                Status = BS->HandleProtocol(DiskHandle, &BlockIoProtocol, (VOID **) &DiskBlockIO);
                if (!EFI_ERROR(Status)) {
                    
                    // check the media block size
                    if (DiskBlockIO->Media->BlockSize == 2048)
                        Volume->DiskKind = DISK_KIND_OPTICAL;
                } //else
                  //  CheckError(Status, L"from HandleProtocol");
            } //else
              //  CheckError(Status, L"from LocateDevicePath");
        }
        
        DevicePath = NextDevicePath;
    }
    
    // default volume icon based on disk kind
    if (Volume->DiskKind == DISK_KIND_INTERNAL)
        Volume->VolBadgeImage = BuiltinIcon(8);
    else if (Volume->DiskKind == DISK_KIND_EXTERNAL)
        Volume->VolBadgeImage = BuiltinIcon(9);
    else if (Volume->DiskKind == DISK_KIND_OPTICAL)
        Volume->VolBadgeImage = BuiltinIcon(10);
    
    // open the root directory of the volume
    Volume->RootDir = LibOpenRoot(Volume->DeviceHandle);
    if (Volume->RootDir == NULL) {
        Print(L"Error: Can't open volume.\n");
        // TODO: signal that we had an error
        return;
    }
    
    // get volume name
    FileSystemInfoPtr = LibFileSystemInfo(Volume->RootDir);
    if (FileSystemInfoPtr != NULL) {
        Print(L"  Volume %s\n", FileSystemInfoPtr->VolumeLabel);
        Volume->VolName = StrDuplicate(FileSystemInfoPtr->VolumeLabel);
        FreePool(FileSystemInfoPtr);
    } else {
        Print(L"Warning: Can't get volume info.\n");
        return;
        // NOTE: this is normal for Apple's VenMedia device paths
    }
    
    // get custom volume icon if present
    if (FileExists(Volume->RootDir, L".VolumeIcon.icns"))
        Volume->VolBadgeImage = LoadIcns(Volume->RootDir, L".VolumeIcon.icns", 32);
}

VOID ScanVolumes(VOID)
{
    EFI_STATUS              Status;
    UINTN                   HandleCount = 0;
    UINTN                   HandleIndex;
    EFI_HANDLE              *Handles;
    REFIT_VOLUME            *Volume;
    
    Print(L"Scanning volumes...\n");
    
    // get all filesystem handles
    Status = LibLocateHandle(ByProtocol, &FileSystemProtocol, NULL, &HandleCount, &Handles);
    // TODO: actually search for BlockIO instead
    if (Status == EFI_NOT_FOUND)
        return;  // no filesystems. strange, but true...
    if (CheckError(Status, L"while listing all file systems"))
        return;
    
    // iterate over the filesystem handles
    for (HandleIndex = 0; HandleIndex < HandleCount; HandleIndex++) {
        
        Volume = AllocateZeroPool(sizeof(REFIT_VOLUME));
        Volume->DeviceHandle = Handles[HandleIndex];
        ScanVolume(Volume);
        
        AddListElement((VOID ***) &Volumes, &VolumesCount, Volume);
        
        if (Volume->DeviceHandle == SelfLoadedImage->DeviceHandle)
            SelfVolume = Volume;
        
    }
    
    if (SelfVolume == NULL)
        Print(L"WARNING: SelfVolume not found");
    
    FreePool(Handles);
}

//
// file and dir functions
//

BOOLEAN FileExists(IN EFI_FILE *BaseDir, IN CHAR16 *RelativePath)
{
    EFI_STATUS  Status;
    EFI_FILE    *TestFile;
    
    Status = BaseDir->Open(BaseDir, &TestFile, RelativePath, EFI_FILE_MODE_READ, 0);
    if (Status == EFI_SUCCESS) {
        TestFile->Close(TestFile);
        return TRUE;
    }
    return FALSE;
}

EFI_STATUS DirNextEntry(IN EFI_FILE *Directory, IN OUT EFI_FILE_INFO **DirEntry, IN UINTN FilterMode)
{
    EFI_STATUS Status;
    VOID *Buffer;
    UINTN InitialBufferSize, BufferSize;
    
    for (;;) {
        
        // free pointer from last call
        if (*DirEntry != NULL) {
            FreePool(*DirEntry);
            *DirEntry = NULL;
        }
        
        // read next directory entry
        BufferSize = InitialBufferSize = 256;
        Buffer = AllocatePool(BufferSize);
        Status = Directory->Read(Directory, &BufferSize, Buffer);
        if (Status == EFI_BUFFER_TOO_SMALL) {
            Buffer = ReallocatePool(Buffer, InitialBufferSize, BufferSize);
            Status = Directory->Read(Directory, &BufferSize, Buffer);
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
    CHAR16 *FileName;
    UINTN  i;
    
    FileName = Path;
    
    if (Path != NULL) {
        for (i = StrLen(Path); i >= 0; i--) {
            if (Path[i] == '\\' || Path[i] == '/') {
                FileName = Path + i + 1;
                break;
            }
        }
    }
    
    return FileName;
}

VOID ReplaceExtension(IN OUT CHAR16 *Path, IN CHAR16 *Extension)
{
    UINTN i;
    
    for (i = StrLen(Path); i >= 0; i--) {
        if (Path[i] == '.') {
            Path[i] = 0;
            break;
        }
        if (Path[i] == '\\' || Path[i] == '/')
            break;
    }
    StrCat(Path, Extension);
}
