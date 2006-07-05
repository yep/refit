/**
 * \file fsw_efi.c
 * EFI host environment code.
 */

/*-
 * Copyright (c) 2006 Christoph Pfisterer
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

#include "fsw_efi.h"

#define DEBUG_LEVEL 0


#ifndef FSTYPE
#define FSTYPE ext2
#endif

#define FSW_EFI_STRINGIFY(x) L#x
#define FSW_EFI_DRIVER_NAME(t) L"Fsw " FSW_EFI_STRINGIFY(t) L" File System Driver"

// functions

EFI_STATUS EFIAPI FswDriverBindingSupported(IN EFI_DRIVER_BINDING_PROTOCOL  *This,
                                            IN EFI_HANDLE                   ControllerHandle,
                                            IN EFI_DEVICE_PATH_PROTOCOL     *RemainingDevicePath);
EFI_STATUS EFIAPI FswDriverBindingStart(IN EFI_DRIVER_BINDING_PROTOCOL  *This,
                                        IN EFI_HANDLE                   ControllerHandle,
                                        IN EFI_DEVICE_PATH_PROTOCOL     *RemainingDevicePath);
EFI_STATUS EFIAPI FswDriverBindingStop(IN  EFI_DRIVER_BINDING_PROTOCOL  *This,
                                       IN  EFI_HANDLE                   ControllerHandle,
                                       IN  UINTN                        NumberOfChildren,
                                       IN  EFI_HANDLE                   *ChildHandleBuffer);

EFI_STATUS EFIAPI FswComponentNameGetDriverName(IN  EFI_COMPONENT_NAME_PROTOCOL  *This,
                                                IN  CHAR8                        *Language,
                                                OUT CHAR16                       **DriverName);
EFI_STATUS EFIAPI FswComponentNameGetControllerName(IN  EFI_COMPONENT_NAME_PROTOCOL    *This,
                                                    IN  EFI_HANDLE                     ControllerHandle,
                                                    IN  EFI_HANDLE                     ChildHandle  OPTIONAL,
                                                    IN  CHAR8                          *Language,
                                                    OUT CHAR16                         **ControllerName);

void fsw_efi_change_blocksize(struct fsw_volume *vol,
                              fsw_u32 old_phys_blocksize, fsw_u32 old_log_blocksize,
                              fsw_u32 new_phys_blocksize, fsw_u32 new_log_blocksize);
fsw_status_t fsw_efi_read_block(struct fsw_volume *vol, fsw_u32 phys_bno, void **buffer_out);

EFI_STATUS FswMapStatus(fsw_status_t fsw_status, FSW_VOLUME_DATA *Volume);

EFI_STATUS EFIAPI FswOpenVolume(IN EFI_FILE_IO_INTERFACE *This,
                                OUT EFI_FILE **Root);
EFI_STATUS FswFileHandleFromDnode(IN struct fsw_dnode *dno,
                                  OUT EFI_FILE **NewFileHandle);

EFI_STATUS FswFileRead(IN FSW_FILE_DATA *File,
                       IN OUT UINTN *BufferSize,
                       OUT VOID *Buffer);
EFI_STATUS FswFileGetPosition(IN FSW_FILE_DATA *File,
                              OUT UINT64 *Position);
EFI_STATUS FswFileSetPosition(IN FSW_FILE_DATA *File,
                              IN UINT64 Position);
EFI_STATUS FswDirOpen(IN FSW_FILE_DATA *File,
                      OUT EFI_FILE **NewHandle,
                      IN CHAR16 *FileName,
                      IN UINT64 OpenMode,
                      IN UINT64 Attributes);
EFI_STATUS FswDirRead(IN FSW_FILE_DATA *File,
                      IN OUT UINTN *BufferSize,
                      OUT VOID *Buffer);
EFI_STATUS FswDirSetPosition(IN FSW_FILE_DATA *File,
                             IN UINT64 Position);
EFI_STATUS FswFileGetInfo(IN FSW_FILE_DATA *File,
                          IN EFI_GUID *InformationType,
                          IN OUT UINTN *BufferSize,
                          OUT VOID *Buffer);

EFI_STATUS FswFillFileInfo(IN FSW_VOLUME_DATA *Volume,
                           IN struct fsw_dnode *dno,
                           IN OUT UINTN *BufferSize,
                           OUT VOID *Buffer);
UINTN FswStringSize(struct fsw_string *s);
VOID FswStringCopy(CHAR16 *Dest, struct fsw_string *src);

// EFI interface structures

EFI_DRIVER_BINDING_PROTOCOL gFswDriverBinding = {
    FswDriverBindingSupported,
    FswDriverBindingStart,
    FswDriverBindingStop,
    0x10,
    NULL,
    NULL
};

EFI_COMPONENT_NAME_PROTOCOL gFswComponentName = {
    FswComponentNameGetDriverName,
    FswComponentNameGetControllerName,
    "eng"
};

// FSW interface structures

struct fsw_host_table   fsw_efi_host_table = {
    FSW_STRING_TYPE_UTF16,
    
    fsw_efi_change_blocksize,
    fsw_efi_read_block
};

extern struct fsw_fstype_table   FSW_FSTYPE_TABLE_NAME(FSTYPE);


//
// EFI image entry point
//
// Installs the Driver Binding and Component Name protocols on the image's handle.
// Mounting a file system is initiated through Driver Binding at the firmware's
// request.
//

EFI_DRIVER_ENTRY_POINT(FswEntryPoint)

EFI_STATUS EFIAPI FswEntryPoint(IN EFI_HANDLE         ImageHandle,
                                IN EFI_SYSTEM_TABLE   *SystemTable)
{
    EFI_STATUS  Status;
    
    InitializeLib(ImageHandle, SystemTable);
    
    // complete Driver Binding protocol instance
    gFswDriverBinding.ImageHandle          = ImageHandle;
    gFswDriverBinding.DriverBindingHandle  = ImageHandle;
    // install Driver Binding protocol
    Status = BS->InstallProtocolInterface(&gFswDriverBinding.DriverBindingHandle,
                                          &DriverBindingProtocol,
                                          EFI_NATIVE_INTERFACE,
                                          &gFswDriverBinding);
    if (EFI_ERROR (Status)) {
        return Status;
    }
    
    // install Component Name protocol
    Status = BS->InstallProtocolInterface(&gFswDriverBinding.DriverBindingHandle,
                                          &ComponentNameProtocol,
                                          EFI_NATIVE_INTERFACE,
                                          &gFswComponentName);
    if (EFI_ERROR (Status)) {
        return Status;
    }
    
    return EFI_SUCCESS;
}

//
// EFI Driver Binding interface: check if we support a "controller"
//

EFI_STATUS EFIAPI FswDriverBindingSupported(IN EFI_DRIVER_BINDING_PROTOCOL  *This,
                                            IN EFI_HANDLE                   ControllerHandle,
                                            IN EFI_DEVICE_PATH_PROTOCOL     *RemainingDevicePath)
{
    EFI_STATUS          Status;
    EFI_DISK_IO         *DiskIo;
    
    // we check for both DiskIO and BlockIO protocols
    
    // first, open DiskIO
    Status = BS->OpenProtocol(ControllerHandle,
                              &DiskIoProtocol,
                              (VOID **) &DiskIo,
                              This->DriverBindingHandle,
                              ControllerHandle,
                              EFI_OPEN_PROTOCOL_BY_DRIVER);
    if (EFI_ERROR(Status))
        return Status;
    
    // we were just checking, close it again
    BS->CloseProtocol(ControllerHandle,
                      &DiskIoProtocol,
                      This->DriverBindingHandle,
                      ControllerHandle);
    
    // next, check BlockIO without actually opening it
    Status = BS->OpenProtocol(ControllerHandle,
                              &BlockIoProtocol,
                              NULL,
                              This->DriverBindingHandle,
                              ControllerHandle,
                              EFI_OPEN_PROTOCOL_TEST_PROTOCOL);
    return Status;
}

//
// EFI Driver Binding interface: start this driver on the given "controller"
//

EFI_STATUS EFIAPI FswDriverBindingStart(IN EFI_DRIVER_BINDING_PROTOCOL  *This,
                                        IN EFI_HANDLE                   ControllerHandle,
                                        IN EFI_DEVICE_PATH_PROTOCOL     *RemainingDevicePath)
{
    EFI_STATUS          Status;
    EFI_BLOCK_IO        *BlockIo;
    EFI_DISK_IO         *DiskIo;
    FSW_VOLUME_DATA     *Volume;
    
#if DEBUG_LEVEL
    Print(L"FswDriverBindingStart\n");
#endif
    
    // open consumed protocols
    Status = BS->OpenProtocol(ControllerHandle,
                              &BlockIoProtocol,
                              (VOID **) &BlockIo,
                              This->DriverBindingHandle,
                              ControllerHandle,
                              EFI_OPEN_PROTOCOL_GET_PROTOCOL);   // NOTE: we only want to look at the MediaId
    if (EFI_ERROR(Status)) {
        Print(L"Fsw ERROR: OpenProtocol(BlockIo) returned %x\n", Status);
        return Status;
    }
    
    Status = BS->OpenProtocol(ControllerHandle,
                              &DiskIoProtocol,
                              (VOID **) &DiskIo,
                              This->DriverBindingHandle,
                              ControllerHandle,
                              EFI_OPEN_PROTOCOL_BY_DRIVER);
    if (EFI_ERROR(Status)) {
        Print(L"Fsw ERROR: OpenProtocol(DiskIo) returned %x\n", Status);
        return Status;
    }
    
    // allocate volume structure
    Volume = AllocateZeroPool(sizeof(FSW_VOLUME_DATA));
    Volume->Signature       = FSW_VOLUME_DATA_SIGNATURE;
    Volume->Handle          = ControllerHandle;
    Volume->DiskIo          = DiskIo;
    Volume->MediaId         = BlockIo->Media->MediaId;
    Volume->LastIOStatus    = EFI_SUCCESS;
    
    // mount the filesystem
    Status = FswMapStatus(fsw_mount(Volume, &fsw_efi_host_table,
                                    &FSW_FSTYPE_TABLE_NAME(FSTYPE), &Volume->vol),
                          Volume);
    
    if (!EFI_ERROR(Status)) {
        // register the SimpleFileSystem protocol
        Volume->FileSystem.Revision     = EFI_FILE_IO_INTERFACE_REVISION;
        Volume->FileSystem.OpenVolume   = FswOpenVolume;
        Status = BS->InstallMultipleProtocolInterfaces(&ControllerHandle,
                                                       &FileSystemProtocol, &Volume->FileSystem,
                                                       NULL);
        if (EFI_ERROR(Status))
            Print(L"Fsw ERROR: InstallMultipleProtocolInterfaces returned %x\n", Status);
    }
    
    // on errors, close the opened protocols
    if (EFI_ERROR(Status)) {
        if (Volume->vol != NULL)
            fsw_unmount(Volume->vol);
        if (Volume->BlockBuffer != NULL)
            FreePool(Volume->BlockBuffer);
        FreePool(Volume);
        
        BS->CloseProtocol(ControllerHandle,
                          &DiskIoProtocol,
                          This->DriverBindingHandle,
                          ControllerHandle);
    }
    
    return Status;
}

//
// EFI Driver Binding interface: stop this driver on the given "controller"
//

EFI_STATUS EFIAPI FswDriverBindingStop(IN  EFI_DRIVER_BINDING_PROTOCOL  *This,
                                       IN  EFI_HANDLE                   ControllerHandle,
                                       IN  UINTN                        NumberOfChildren,
                                       IN  EFI_HANDLE                   *ChildHandleBuffer)
{
    EFI_STATUS          Status;
    EFI_FILE_IO_INTERFACE *FileSystem;
    FSW_VOLUME_DATA     *Volume;
    
#if DEBUG_LEVEL
    Print(L"FswDriverBindingStop\n");
#endif
    
    // get the installed SimpleFileSystem interface
    Status = BS->OpenProtocol(ControllerHandle,
                              &FileSystemProtocol,
                              (VOID **) &FileSystem,
                              This->DriverBindingHandle,
                              ControllerHandle,
                              EFI_OPEN_PROTOCOL_GET_PROTOCOL);
    if (EFI_ERROR(Status))
        return EFI_UNSUPPORTED;
    
    // get private data structure
    Volume = FSW_VOLUME_FROM_FILE_SYSTEM(FileSystem);
    
    // uninstall Simple File System protocol
    Status = BS->UninstallMultipleProtocolInterfaces(ControllerHandle,
                                                     &FileSystemProtocol, &Volume->FileSystem,
                                                     NULL);
    if (EFI_ERROR(Status)) {
        Print(L"Fsw ERROR: UninstallMultipleProtocolInterfaces returned %x\n", Status);
        return Status;
    }
#if DEBUG_LEVEL
    Print(L"FswDriverBindingStop: protocol uninstalled successfully\n");
#endif
    
    // release private data structure
    if (Volume->vol != NULL)
        fsw_unmount(Volume->vol);
    if (Volume->BlockBuffer != NULL)
        FreePool(Volume->BlockBuffer);
    FreePool(Volume);
    
    // close the consumed protocols
    Status = BS->CloseProtocol(ControllerHandle,
                               &DiskIoProtocol,
                               This->DriverBindingHandle,
                               ControllerHandle);
    
    return Status;
}

//
// EFI Component Name protocol interface
//

EFI_STATUS EFIAPI FswComponentNameGetDriverName(IN  EFI_COMPONENT_NAME_PROTOCOL  *This,
                                                IN  CHAR8                        *Language,
                                                OUT CHAR16                       **DriverName)
{
    if (Language == NULL || DriverName == NULL)
        return EFI_INVALID_PARAMETER;
    
    if (Language[0] == 'e' && Language[1] == 'n' && Language[2] == 'g' && Language[3] == 0) {
        *DriverName = FSW_EFI_DRIVER_NAME(FSTYPE);
        return EFI_SUCCESS;
    }
    return EFI_UNSUPPORTED;
}

EFI_STATUS EFIAPI FswComponentNameGetControllerName(IN  EFI_COMPONENT_NAME_PROTOCOL    *This,
                                                    IN  EFI_HANDLE                     ControllerHandle,
                                                    IN  EFI_HANDLE                     ChildHandle  OPTIONAL,
                                                    IN  CHAR8                          *Language,
                                                    OUT CHAR16                         **ControllerName)
{
    return EFI_UNSUPPORTED;
}

//
// FSW interface functions
//

void fsw_efi_change_blocksize(struct fsw_volume *vol,
                              fsw_u32 old_phys_blocksize, fsw_u32 old_log_blocksize,
                              fsw_u32 new_phys_blocksize, fsw_u32 new_log_blocksize)
{
    FSW_VOLUME_DATA     *Volume = (FSW_VOLUME_DATA *)vol->host_data;
    
    if (Volume->BlockBuffer != NULL) {
        FreePool(Volume->BlockBuffer);
        Volume->BlockBuffer = NULL;
    }
    Volume->BlockInBuffer = INVALID_BLOCK_NO;
}

fsw_status_t fsw_efi_read_block(struct fsw_volume *vol, fsw_u32 phys_bno, void **buffer_out)
{
    EFI_STATUS          Status;
    FSW_VOLUME_DATA     *Volume = (FSW_VOLUME_DATA *)vol->host_data;
    
    // check buffer
    if (phys_bno == Volume->BlockInBuffer) {
        *buffer_out = Volume->BlockBuffer;
        return FSW_SUCCESS;
    }
    
#if DEBUG_LEVEL == 2
    Print(L"FswReadBlock: %d  (%d)\n", phys_bno, vol->phys_blocksize);
#endif
    
    // allocate buffer if necessary
    if (Volume->BlockBuffer == NULL) {
        Volume->BlockBuffer = AllocatePool(vol->phys_blocksize);
        if (Volume->BlockBuffer == NULL)
            return FSW_OUT_OF_MEMORY;
    }
    
    // read from disk
    Status = Volume->DiskIo->ReadDisk(Volume->DiskIo, Volume->MediaId,
                                      (UINT64)phys_bno * vol->phys_blocksize,
                                      vol->phys_blocksize,
                                      Volume->BlockBuffer);
    Volume->LastIOStatus = Status;
    if (EFI_ERROR(Status)) {
        Volume->BlockInBuffer = INVALID_BLOCK_NO;
        return FSW_IO_ERROR;
    }
    Volume->BlockInBuffer = phys_bno;
    *buffer_out = Volume->BlockBuffer;
    
    return FSW_SUCCESS;
}

//
// FSW error code mapping
//

EFI_STATUS FswMapStatus(fsw_status_t fsw_status, FSW_VOLUME_DATA *Volume)
{
    switch (fsw_status) {
        case FSW_SUCCESS:
            return EFI_SUCCESS;
        case FSW_OUT_OF_MEMORY:
            return EFI_VOLUME_CORRUPTED;
        case FSW_IO_ERROR:
            return Volume->LastIOStatus;
        case FSW_UNSUPPORTED:
            return EFI_UNSUPPORTED;
        case FSW_NOT_FOUND:
            return EFI_NOT_FOUND;
        case FSW_VOLUME_CORRUPTED:
            return EFI_VOLUME_CORRUPTED;
        default:
            return EFI_DEVICE_ERROR;
    }
}

//
// EFI File System interface
//

EFI_STATUS EFIAPI FswOpenVolume(IN EFI_FILE_IO_INTERFACE *This,
                                OUT EFI_FILE **Root)
{
    EFI_STATUS          Status;
    FSW_VOLUME_DATA     *Volume = FSW_VOLUME_FROM_FILE_SYSTEM(This);
    
#if DEBUG_LEVEL
    Print(L"FswOpenVolume\n");
#endif
    
    Status = FswFileHandleFromDnode(Volume->vol->root, Root);
    
    return Status;
}

//
// EFI File Handle interface (dispatching only)
//

EFI_STATUS EFIAPI FswFileHandleOpen(IN EFI_FILE *This,
                                    OUT EFI_FILE **NewHandle,
                                    IN CHAR16 *FileName,
                                    IN UINT64 OpenMode,
                                    IN UINT64 Attributes)
{
    FSW_FILE_DATA      *File = FSW_FILE_FROM_FILE_HANDLE(This);
    
    if (File->Type == FSW_EFI_FILE_TYPE_DIR)
        return FswDirOpen(File, NewHandle, FileName, OpenMode, Attributes);
    // not supported for regular files
    return EFI_UNSUPPORTED;
}

EFI_STATUS EFIAPI FswFileHandleClose(IN EFI_FILE *This)
{
    FSW_FILE_DATA      *File = FSW_FILE_FROM_FILE_HANDLE(This);
    
#if DEBUG_LEVEL
    Print(L"FswFileClose\n");
#endif
    
    fsw_shandle_close(&File->shand);
    FreePool(File);
    
    return EFI_SUCCESS;
}

EFI_STATUS EFIAPI FswFileHandleDelete(IN EFI_FILE *This)
{
    EFI_STATUS          Status;
    
    Status = This->Close(This);
    if (Status == EFI_SUCCESS) {
        // this driver is read-only
        Status = EFI_WARN_DELETE_FAILURE;
    }
    
    return Status;
}

EFI_STATUS EFIAPI FswFileHandleRead(IN EFI_FILE *This,
                                    IN OUT UINTN *BufferSize,
                                    OUT VOID *Buffer)
{
    FSW_FILE_DATA      *File = FSW_FILE_FROM_FILE_HANDLE(This);
    
    if (File->Type == FSW_EFI_FILE_TYPE_FILE)
        return FswFileRead(File, BufferSize, Buffer);
    else if (File->Type == FSW_EFI_FILE_TYPE_DIR)
        return FswDirRead(File, BufferSize, Buffer);
    return EFI_UNSUPPORTED;
}

EFI_STATUS EFIAPI FswFileHandleWrite(IN EFI_FILE *This,
                                     IN OUT UINTN *BufferSize,
                                     IN VOID *Buffer)
{
    // this driver is read-only
    return EFI_WRITE_PROTECTED;
}

EFI_STATUS EFIAPI FswFileHandleGetPosition(IN EFI_FILE *This,
                                           OUT UINT64 *Position)
{
    FSW_FILE_DATA      *File = FSW_FILE_FROM_FILE_HANDLE(This);
    
    if (File->Type == FSW_EFI_FILE_TYPE_FILE)
        return FswFileGetPosition(File, Position);
    // not defined for directories
    return EFI_UNSUPPORTED;
}

EFI_STATUS EFIAPI FswFileHandleSetPosition(IN EFI_FILE *This,
                                           IN UINT64 Position)
{
    FSW_FILE_DATA      *File = FSW_FILE_FROM_FILE_HANDLE(This);
    
    if (File->Type == FSW_EFI_FILE_TYPE_FILE)
        return FswFileSetPosition(File, Position);
    else if (File->Type == FSW_EFI_FILE_TYPE_DIR)
        return FswDirSetPosition(File, Position);
    return EFI_UNSUPPORTED;
}

EFI_STATUS EFIAPI FswFileHandleGetInfo(IN EFI_FILE *This,
                                       IN EFI_GUID *InformationType,
                                       IN OUT UINTN *BufferSize,
                                       OUT VOID *Buffer)
{
    FSW_FILE_DATA      *File = FSW_FILE_FROM_FILE_HANDLE(This);
    
    return FswFileGetInfo(File, InformationType, BufferSize, Buffer);
}

EFI_STATUS EFIAPI FswFileHandleSetInfo(IN EFI_FILE *This,
                                       IN EFI_GUID *InformationType,
                                       IN UINTN BufferSize,
                                       IN VOID *Buffer)
{
    // this driver is read-only
    return EFI_WRITE_PROTECTED;
}

EFI_STATUS EFIAPI FswFileHandleFlush(IN EFI_FILE *This)
{
    // this driver is read-only
    return EFI_WRITE_PROTECTED;
}

//
// Create file handle for a dnode
//

EFI_STATUS FswFileHandleFromDnode(IN struct fsw_dnode *dno,
                                  OUT EFI_FILE **NewFileHandle)
{
    EFI_STATUS          Status;
    FSW_FILE_DATA       *File;
    
    // make sure the dnode has complete info
    Status = FswMapStatus(fsw_dnode_fill(dno), (FSW_VOLUME_DATA *)dno->vol->host_data);
    if (EFI_ERROR(Status))
        return Status;
    
    // check type
    if (dno->type != FSW_DNODE_TYPE_FILE && dno->type != FSW_DNODE_TYPE_DIR)
        return EFI_UNSUPPORTED;
    
    // allocate file structure
    File = AllocateZeroPool(sizeof(FSW_FILE_DATA));
    File->Signature = FSW_FILE_DATA_SIGNATURE;
    if (dno->type == FSW_DNODE_TYPE_FILE)
        File->Type = FSW_EFI_FILE_TYPE_FILE;
    else if (dno->type == FSW_DNODE_TYPE_DIR)
        File->Type = FSW_EFI_FILE_TYPE_DIR;
    
    // open shandle
    Status = FswMapStatus(fsw_shandle_open(dno, &File->shand),
                          (FSW_VOLUME_DATA *)dno->vol->host_data);
    if (EFI_ERROR(Status)) {
        FreePool(File);
        return Status;
    }
    
    // populate the file handle
    File->FileHandle.Revision    = EFI_FILE_HANDLE_REVISION;
    File->FileHandle.Open        = FswFileHandleOpen;
    File->FileHandle.Close       = FswFileHandleClose;
    File->FileHandle.Delete      = FswFileHandleDelete;
    File->FileHandle.Read        = FswFileHandleRead;
    File->FileHandle.Write       = FswFileHandleWrite;
    File->FileHandle.GetPosition = FswFileHandleGetPosition;
    File->FileHandle.SetPosition = FswFileHandleSetPosition;
    File->FileHandle.GetInfo     = FswFileHandleGetInfo;
    File->FileHandle.SetInfo     = FswFileHandleSetInfo;
    File->FileHandle.Flush       = FswFileHandleFlush;
    
    *NewFileHandle = &File->FileHandle;
    return EFI_SUCCESS;
}

//
// Functions for regular files
//

EFI_STATUS FswFileRead(IN FSW_FILE_DATA *File,
                       IN OUT UINTN *BufferSize,
                       OUT VOID *Buffer)
{
    EFI_STATUS          Status;
    fsw_u32             buffer_size;
    
#if DEBUG_LEVEL
    Print(L"FswFileRead %d bytes\n", *BufferSize);
#endif
    
    buffer_size = *BufferSize;
    Status = FswMapStatus(fsw_shandle_read(&File->shand, &buffer_size, Buffer),
                          (FSW_VOLUME_DATA *)File->shand.dnode->vol->host_data);
    *BufferSize = buffer_size;
    
    return Status;
}

EFI_STATUS FswFileGetPosition(IN FSW_FILE_DATA *File,
                              OUT UINT64 *Position)
{
    *Position = File->shand.pos;
    return EFI_SUCCESS;
}

EFI_STATUS FswFileSetPosition(IN FSW_FILE_DATA *File,
                              IN UINT64 Position)
{
    if (Position == 0xFFFFFFFFFFFFFFFFULL)
        File->shand.pos = File->shand.dnode->size;
    else
        File->shand.pos = Position;
    return EFI_SUCCESS;
}

//
// Functions for directories
//

EFI_STATUS FswDirOpen(IN FSW_FILE_DATA *File,
                      OUT EFI_FILE **NewHandle,
                      IN CHAR16 *FileName,
                      IN UINT64 OpenMode,
                      IN UINT64 Attributes)
{
    EFI_STATUS          Status;
    FSW_VOLUME_DATA     *Volume = (FSW_VOLUME_DATA *)File->shand.dnode->vol->host_data;
    struct fsw_dnode    *dno;
    struct fsw_dnode    *target_dno;
    /*
    CHAR16              *PathElement;
    CHAR16              *PathElementEnd;
    CHAR16              *NextPathElement;
    struct fsw_string   lookup_name;
    */
    struct fsw_string   lookup_path;
    
#if DEBUG_LEVEL
    Print(L"FswDirOpen: '%s'\n", FileName);
#endif
    
    if (OpenMode != EFI_FILE_MODE_READ)
        return EFI_WRITE_PROTECTED;
    
    lookup_path.type = FSW_STRING_TYPE_UTF16;
    lookup_path.len  = StrLen(FileName);
    lookup_path.size = lookup_path.len * sizeof(fsw_u16);
    lookup_path.data = FileName;
    
    // resolve the path (symlinks along the way are automatically resolved)
    Status = FswMapStatus(fsw_dnode_lookup_path(File->shand.dnode, &lookup_path, '\\', &dno),
                          Volume);
    if (EFI_ERROR(Status))
        return Status;
    
    // if the final node is a symlink, also resolve it
    Status = FswMapStatus(fsw_dnode_resolve(dno, &target_dno),
                          Volume);
    fsw_dnode_release(dno);
    if (EFI_ERROR(Status))
        return Status;
    dno = target_dno;
    
    // make a new EFI handle for the target dnode
    Status = FswFileHandleFromDnode(dno, NewHandle);
    fsw_dnode_release(dno);
    return Status;
    
    
    /*
    
    // analyze start of path, pick starting point
    PathElement = FileName;
    if (*PathElement == '\\') {
        dno = Volume->vol->root;
        while (*PathElement == '\\')
            PathElement++;
    } else
        dno = File->shand.dnode;
    fsw_dnode_retain(dno);
    
    // loop over the path
    for (; *PathElement != 0; PathElement = NextPathElement) {
        // parse next path element
        PathElementEnd = PathElement;
        while (*PathElementEnd != 0 && *PathElementEnd != '\\')
            PathElementEnd++;
        NextPathElement = PathElementEnd;
        while (*NextPathElement == '\\')
            NextPathElement++;
        lookup_name.type = FSW_STRING_TYPE_UTF16;
        lookup_name.len  = PathElementEnd - PathElement;
        lookup_name.size = lookup_name.len * sizeof(fsw_u16);
        lookup_name.data = PathElement;
        
        // make sure the dnode has complete info
        Status = FswMapStatus(fsw_dnode_fill(dno), Volume);
        if (EFI_ERROR(Status))
            goto errorexit;
        
        // check that this actually is a directory
        if (dno->type != FSW_DNODE_TYPE_DIR) {
#if DEBUG_LEVEL == 2
            Print(L"FswDirOpen: NOT FOUND (not a directory)\n");
#endif
            Status = EFI_NOT_FOUND;
            goto errorexit;
        }
        
        // check for . and ..
        if (lookup_name.len == 1 && PathElement[0] == '.') {
            child_dno = dno;
            fsw_dnode_retain(child_dno);
        } else if (lookup_name.len == 2 && PathElement[0] == '.' && PathElement[1] == '.') {
            if (dno->parent == NULL) {
                // We're at the root directory and trying to go up. The EFI spec says that this
                // is not a valid operation and the EFI shell relies on it.
                Status = EFI_NOT_FOUND;
                goto errorexit;
            }
            child_dno = dno->parent;
            fsw_dnode_retain(child_dno);
        } else {
            Status = FswMapStatus(fsw_dnode_lookup(dno, &lookup_name, &child_dno),
                                  Volume);
            if (EFI_ERROR(Status))
                goto errorexit;
        }
        
        // resolve symbolic link
        if (child_dno->type == FSW_DNODE_TYPE_SYMLINK) {
            // TODO
        }
        
        // child_dno becomes the new dno
        fsw_dnode_release(dno);
        dno = child_dno;   // is already retained
        child_dno = NULL;
    }
    
    // make a new EFI handle for the target dnode
    Status = FswFileHandleFromDnode(dno, NewHandle);
    
errorexit:
    fsw_dnode_release(dno);
    if (child_dno != NULL)
        fsw_dnode_release(child_dno);
    return Status;
     */
}

//
// EFI_FILE Read for directories
//

EFI_STATUS FswDirRead(IN FSW_FILE_DATA *File,
                      IN OUT UINTN *BufferSize,
                      OUT VOID *Buffer)
{
    EFI_STATUS          Status;
    FSW_VOLUME_DATA     *Volume = (FSW_VOLUME_DATA *)File->shand.dnode->vol->host_data;
    struct fsw_dnode    *dno;
    
#if DEBUG_LEVEL
    Print(L"FswDirRead...\n");
#endif
    
    // read the next entry
    Status = FswMapStatus(fsw_dnode_dir_read(&File->shand, &dno),
                          Volume);
    if (Status == EFI_NOT_FOUND) {
        // end of directory
        *BufferSize = 0;
#if DEBUG_LEVEL
        Print(L"...no more entries\n");
#endif
        return EFI_SUCCESS;
    }
    if (EFI_ERROR(Status))
        return Status;
    
    // get info into buffer
    Status = FswFillFileInfo(Volume, dno, BufferSize, Buffer);
    fsw_dnode_release(dno);
    return Status;
}

//
// EFI_FILE SetPosition for directories
//

EFI_STATUS FswDirSetPosition(IN FSW_FILE_DATA *File,
                             IN UINT64 Position)
{
    if (Position == 0) {
        File->shand.pos = 0;
        return EFI_SUCCESS;
    } else {
        // directories can only rewind to the start
        return EFI_UNSUPPORTED;
    }
}

//
// Functions for both files and directories
//

EFI_STATUS FswFileGetInfo(IN FSW_FILE_DATA *File,
                          IN EFI_GUID *InformationType,
                          IN OUT UINTN *BufferSize,
                          OUT VOID *Buffer)
{
    EFI_STATUS          Status;
    FSW_VOLUME_DATA     *Volume = (FSW_VOLUME_DATA *)File->shand.dnode->vol->host_data;
    EFI_FILE_SYSTEM_INFO *FSInfo;
    UINTN               RequiredSize;
    struct fsw_volume_stat vsb;
    
    if (CompareGuid(InformationType, &GenericFileInfo) == 0) {
#if DEBUG_LEVEL
        Print(L"FswFileGetInfo: FILE_INFO\n");
#endif
        
        Status = FswFillFileInfo(Volume, File->shand.dnode, BufferSize, Buffer);
        
    } else if (CompareGuid(InformationType, &FileSystemInfo) == 0) {
#if DEBUG_LEVEL
        Print(L"FswFileGetInfo: FILE_SYSTEM_INFO\n");
#endif
        
        // check buffer size
        RequiredSize = SIZE_OF_EFI_FILE_SYSTEM_INFO + FswStringSize(&Volume->vol->label);
        if (*BufferSize < RequiredSize) {
            *BufferSize = RequiredSize;
            return EFI_BUFFER_TOO_SMALL;
        }
        
        // fill structure
        FSInfo = (EFI_FILE_SYSTEM_INFO *)Buffer;
        FSInfo->Size        = RequiredSize;
        FSInfo->ReadOnly    = TRUE;
        FSInfo->BlockSize   = Volume->vol->log_blocksize;
        FswStringCopy(FSInfo->VolumeLabel, &Volume->vol->label);
        
        // get the missing info from the fs driver
        ZeroMem(&vsb, sizeof(struct fsw_volume_stat));
        Status = FswMapStatus(fsw_volume_stat(Volume->vol, &vsb), Volume);
        if (EFI_ERROR(Status))
            return Status;
        FSInfo->VolumeSize  = vsb.total_bytes;
        FSInfo->FreeSpace   = vsb.free_bytes;
        
        // prepare for return
        *BufferSize = RequiredSize;
        Status = EFI_SUCCESS;
        
    } else if (CompareGuid(InformationType, &FileSystemVolumeLabelInfo) == 0) {
#if DEBUG_LEVEL
        Print(L"FswFileGetInfo: FILE_SYSTEM_VOLUME_LABEL\n");
#endif
        
        // check buffer size
        RequiredSize = SIZE_OF_EFI_FILE_SYSTEM_VOLUME_LABEL_INFO + FswStringSize(&Volume->vol->label);
        if (*BufferSize < RequiredSize) {
            *BufferSize = RequiredSize;
            return EFI_BUFFER_TOO_SMALL;
        }
        
        // copy volume label
        FswStringCopy(((EFI_FILE_SYSTEM_VOLUME_LABEL_INFO *)Buffer)->VolumeLabel, &Volume->vol->label);
        
        // prepare for return
        *BufferSize = RequiredSize;
        Status = EFI_SUCCESS;
        
    } else {
        Status = EFI_UNSUPPORTED;
    }
    
    return Status;
}

//
// time conversion
//
// Adopted from public domain code in FreeBSD libc.
//

#define SECSPERMIN      60
#define MINSPERHOUR     60
#define HOURSPERDAY     24
#define DAYSPERWEEK     7
#define DAYSPERNYEAR    365
#define DAYSPERLYEAR    366
#define SECSPERHOUR     (SECSPERMIN * MINSPERHOUR)
#define SECSPERDAY      ((long) SECSPERHOUR * HOURSPERDAY)
#define MONSPERYEAR     12

#define EPOCH_YEAR      1970
#define EPOCH_WDAY      TM_THURSDAY

#define isleap(y) (((y) % 4) == 0 && (((y) % 100) != 0 || ((y) % 400) == 0))
#define LEAPS_THRU_END_OF(y)    ((y) / 4 - (y) / 100 + (y) / 400)

static const int mon_lengths[2][MONSPERYEAR] = {
    { 31, 28, 31, 30, 31, 30, 31, 31, 30, 31, 30, 31 },
    { 31, 29, 31, 30, 31, 30, 31, 31, 30, 31, 30, 31 }
};
static const int year_lengths[2] = {
    DAYSPERNYEAR, DAYSPERLYEAR
};

static VOID FswDecodeTime(OUT EFI_TIME *EfiTime, IN UINT32 UnixTime)
{
    long        days, rem;
    int         y, newy, yleap;
    const int   *ip;
    
    ZeroMem(EfiTime, sizeof(EFI_TIME));
    
    days = UnixTime / SECSPERDAY;
    rem = UnixTime % SECSPERDAY;
    
    EfiTime->Hour = (int) (rem / SECSPERHOUR);
    rem = rem % SECSPERHOUR;
    EfiTime->Minute = (int) (rem / SECSPERMIN);
    EfiTime->Second = (int) (rem % SECSPERMIN);
    
    y = EPOCH_YEAR;
    while (days < 0 || days >= (long) year_lengths[yleap = isleap(y)]) {
        newy = y + days / DAYSPERNYEAR;
        if (days < 0)
            --newy;
        days -= (newy - y) * DAYSPERNYEAR +
            LEAPS_THRU_END_OF(newy - 1) -
            LEAPS_THRU_END_OF(y - 1);
        y = newy;
    }
    EfiTime->Year = y;
    ip = mon_lengths[yleap];
    for (EfiTime->Month = 0; days >= (long) ip[EfiTime->Month]; ++(EfiTime->Month))
        days = days - (long) ip[EfiTime->Month];
    EfiTime->Month++;  // adjust range to EFI conventions
    EfiTime->Day = (int) (days + 1);
}

static void FswStoreTimePosix(struct fsw_dnode_stat *sb, int which, fsw_u32 posix_time)
{
    EFI_FILE_INFO       *FileInfo = (EFI_FILE_INFO *)sb->host_data;
    
    if (which == FSW_DNODE_STAT_CTIME)
        FswDecodeTime(&FileInfo->CreateTime,       posix_time);
    else if (which == FSW_DNODE_STAT_MTIME)
        FswDecodeTime(&FileInfo->ModificationTime, posix_time);
    else if (which == FSW_DNODE_STAT_ATIME)
        FswDecodeTime(&FileInfo->LastAccessTime,   posix_time);
}

static void FswStoreAttrPosix(struct fsw_dnode_stat *sb, fsw_u16 posix_mode)
{
    EFI_FILE_INFO       *FileInfo = (EFI_FILE_INFO *)sb->host_data;
    
    if ((posix_mode & S_IWUSR) == 0)
        FileInfo->Attribute |= EFI_FILE_READ_ONLY;
}

EFI_STATUS FswFillFileInfo(IN FSW_VOLUME_DATA *Volume,
                           IN struct fsw_dnode *dno,
                           IN OUT UINTN *BufferSize,
                           OUT VOID *Buffer)
{
    EFI_STATUS          Status;
    EFI_FILE_INFO       *FileInfo;
    UINTN               RequiredSize;
    struct fsw_dnode_stat sb;
    
    // make sure the dnode has complete info
    Status = FswMapStatus(fsw_dnode_fill(dno), Volume);
    if (EFI_ERROR(Status))
        return Status;
    
    // TODO: check/assert that the dno's name is in UTF16
    
    // check buffer size
    RequiredSize = SIZE_OF_EFI_FILE_INFO + FswStringSize(&dno->name);
    if (*BufferSize < RequiredSize) {
        // TODO: wind back the directory in this case
        
#if DEBUG_LEVEL
        Print(L"...BUFFER TOO SMALL\n");
#endif
        *BufferSize = RequiredSize;
        return EFI_BUFFER_TOO_SMALL;
    }
    
    // fill structure
    ZeroMem(Buffer, RequiredSize);
    FileInfo = (EFI_FILE_INFO *)Buffer;
    FileInfo->Size = RequiredSize;
    FileInfo->FileSize          = dno->size;
    FileInfo->Attribute         = 0;
    if (dno->type == FSW_DNODE_TYPE_DIR)
        FileInfo->Attribute    |= EFI_FILE_DIRECTORY;
    FswStringCopy(FileInfo->FileName, &dno->name);
    
    // get the missing info from the fs driver
    ZeroMem(&sb, sizeof(struct fsw_dnode_stat));
    sb.store_time_posix = FswStoreTimePosix;
    sb.store_attr_posix = FswStoreAttrPosix;
    sb.host_data = FileInfo;
    Status = FswMapStatus(fsw_dnode_stat(dno, &sb), Volume);
    if (EFI_ERROR(Status))
        return Status;
    FileInfo->PhysicalSize      = sb.used_bytes;
    
    // prepare for return
    *BufferSize = RequiredSize;
#if DEBUG_LEVEL
    Print(L"...returning '%s'\n", FileInfo->FileName);
#endif
    return EFI_SUCCESS;
}

//
// String functions, used for file and volume info
//

UINTN FswStringSize(struct fsw_string *s)
{
    if (s->type == FSW_STRING_TYPE_EMPTY)
        return sizeof(CHAR16);
    return (s->len + 1) * sizeof(CHAR16);
}

VOID FswStringCopy(CHAR16 *Dest, struct fsw_string *src)
{
    if (src->type == FSW_STRING_TYPE_EMPTY) {
        Dest[0] = 0;
    } else if (src->type == FSW_STRING_TYPE_UTF16) {
        CopyMem(Dest, src->data, src->size);
        Dest[src->len] = 0;
    } else {
        // TODO: coerce, recurse
        Dest[0] = 0;
    }
}

// EOF
