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
/** The file system type name to use. */
#define FSTYPE ext2
#endif

/** Helper macro for stringification. */
#define FSW_EFI_STRINGIFY(x) L#x
/** Expands to the EFI driver name given the file system type name. */
#define FSW_EFI_DRIVER_NAME(t) L"Fsw " FSW_EFI_STRINGIFY(t) L" File System Driver"

// functions

EFI_STATUS EFIAPI fsw_efi_DriverBinding_Supported(IN EFI_DRIVER_BINDING_PROTOCOL  *This,
                                                  IN EFI_HANDLE                   ControllerHandle,
                                                  IN EFI_DEVICE_PATH_PROTOCOL     *RemainingDevicePath);
EFI_STATUS EFIAPI fsw_efi_DriverBinding_Start(IN EFI_DRIVER_BINDING_PROTOCOL  *This,
                                              IN EFI_HANDLE                   ControllerHandle,
                                              IN EFI_DEVICE_PATH_PROTOCOL     *RemainingDevicePath);
EFI_STATUS EFIAPI fsw_efi_DriverBinding_Stop(IN  EFI_DRIVER_BINDING_PROTOCOL  *This,
                                             IN  EFI_HANDLE                   ControllerHandle,
                                             IN  UINTN                        NumberOfChildren,
                                             IN  EFI_HANDLE                   *ChildHandleBuffer);

EFI_STATUS EFIAPI fsw_efi_ComponentName_GetDriverName(IN  EFI_COMPONENT_NAME_PROTOCOL  *This,
                                                      IN  CHAR8                        *Language,
                                                      OUT CHAR16                       **DriverName);
EFI_STATUS EFIAPI fsw_efi_ComponentName_GetControllerName(IN  EFI_COMPONENT_NAME_PROTOCOL    *This,
                                                          IN  EFI_HANDLE                     ControllerHandle,
                                                          IN  EFI_HANDLE                     ChildHandle  OPTIONAL,
                                                          IN  CHAR8                          *Language,
                                                          OUT CHAR16                         **ControllerName);

void fsw_efi_change_blocksize(struct fsw_volume *vol,
                              fsw_u32 old_phys_blocksize, fsw_u32 old_log_blocksize,
                              fsw_u32 new_phys_blocksize, fsw_u32 new_log_blocksize);
fsw_status_t fsw_efi_read_block(struct fsw_volume *vol, fsw_u32 phys_bno, void **buffer_out);

EFI_STATUS fsw_efi_map_status(fsw_status_t fsw_status, FSW_VOLUME_DATA *Volume);

EFI_STATUS EFIAPI fsw_efi_FileSystem_OpenVolume(IN EFI_FILE_IO_INTERFACE *This,
                                                OUT EFI_FILE **Root);
EFI_STATUS fsw_efi_dnode_to_FileHandle(IN struct fsw_dnode *dno,
                                       OUT EFI_FILE **NewFileHandle);

EFI_STATUS fsw_efi_file_read(IN FSW_FILE_DATA *File,
                             IN OUT UINTN *BufferSize,
                             OUT VOID *Buffer);
EFI_STATUS fsw_efi_file_getpos(IN FSW_FILE_DATA *File,
                               OUT UINT64 *Position);
EFI_STATUS fsw_efi_file_setpos(IN FSW_FILE_DATA *File,
                               IN UINT64 Position);

EFI_STATUS fsw_efi_dir_open(IN FSW_FILE_DATA *File,
                            OUT EFI_FILE **NewHandle,
                            IN CHAR16 *FileName,
                            IN UINT64 OpenMode,
                            IN UINT64 Attributes);
EFI_STATUS fsw_efi_dir_read(IN FSW_FILE_DATA *File,
                            IN OUT UINTN *BufferSize,
                            OUT VOID *Buffer);
EFI_STATUS fsw_efi_dir_setpos(IN FSW_FILE_DATA *File,
                              IN UINT64 Position);

EFI_STATUS fsw_efi_dnode_getinfo(IN FSW_FILE_DATA *File,
                                 IN EFI_GUID *InformationType,
                                 IN OUT UINTN *BufferSize,
                                 OUT VOID *Buffer);
EFI_STATUS fsw_efi_dnode_fill_FileInfo(IN FSW_VOLUME_DATA *Volume,
                                       IN struct fsw_dnode *dno,
                                       IN OUT UINTN *BufferSize,
                                       OUT VOID *Buffer);

// EFI interface structures

EFI_DRIVER_BINDING_PROTOCOL fsw_efi_DriverBinding_table = {
    fsw_efi_DriverBinding_Supported,
    fsw_efi_DriverBinding_Start,
    fsw_efi_DriverBinding_Stop,
    0x10,
    NULL,
    NULL
};

EFI_COMPONENT_NAME_PROTOCOL fsw_efi_ComponentName_table = {
    fsw_efi_ComponentName_GetDriverName,
    fsw_efi_ComponentName_GetControllerName,
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

EFI_DRIVER_ENTRY_POINT(fsw_efi_main)

EFI_STATUS EFIAPI fsw_efi_main(IN EFI_HANDLE         ImageHandle,
                               IN EFI_SYSTEM_TABLE   *SystemTable)
{
    EFI_STATUS  Status;
    
    InitializeLib(ImageHandle, SystemTable);
    
    // complete Driver Binding protocol instance
    fsw_efi_DriverBinding_table.ImageHandle          = ImageHandle;
    fsw_efi_DriverBinding_table.DriverBindingHandle  = ImageHandle;
    // install Driver Binding protocol
    Status = BS->InstallProtocolInterface(&fsw_efi_DriverBinding_table.DriverBindingHandle,
                                          &DriverBindingProtocol,
                                          EFI_NATIVE_INTERFACE,
                                          &fsw_efi_DriverBinding_table);
    if (EFI_ERROR (Status)) {
        return Status;
    }
    
    // install Component Name protocol
    Status = BS->InstallProtocolInterface(&fsw_efi_DriverBinding_table.DriverBindingHandle,
                                          &ComponentNameProtocol,
                                          EFI_NATIVE_INTERFACE,
                                          &fsw_efi_ComponentName_table);
    if (EFI_ERROR (Status)) {
        return Status;
    }
    
    return EFI_SUCCESS;
}

//
// EFI Driver Binding interface: check if we support a "controller"
//

EFI_STATUS EFIAPI fsw_efi_DriverBinding_Supported(IN EFI_DRIVER_BINDING_PROTOCOL  *This,
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

EFI_STATUS EFIAPI fsw_efi_DriverBinding_Start(IN EFI_DRIVER_BINDING_PROTOCOL  *This,
                                              IN EFI_HANDLE                   ControllerHandle,
                                              IN EFI_DEVICE_PATH_PROTOCOL     *RemainingDevicePath)
{
    EFI_STATUS          Status;
    EFI_BLOCK_IO        *BlockIo;
    EFI_DISK_IO         *DiskIo;
    FSW_VOLUME_DATA     *Volume;
    
#if DEBUG_LEVEL
    Print(L"fsw_efi_DriverBinding_Start\n");
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
    Status = fsw_efi_map_status(fsw_mount(Volume, &fsw_efi_host_table,
                                          &FSW_FSTYPE_TABLE_NAME(FSTYPE), &Volume->vol),
                                Volume);
    
    if (!EFI_ERROR(Status)) {
        // register the SimpleFileSystem protocol
        Volume->FileSystem.Revision     = EFI_FILE_IO_INTERFACE_REVISION;
        Volume->FileSystem.OpenVolume   = fsw_efi_FileSystem_OpenVolume;
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

EFI_STATUS EFIAPI fsw_efi_DriverBinding_Stop(IN  EFI_DRIVER_BINDING_PROTOCOL  *This,
                                             IN  EFI_HANDLE                   ControllerHandle,
                                             IN  UINTN                        NumberOfChildren,
                                             IN  EFI_HANDLE                   *ChildHandleBuffer)
{
    EFI_STATUS          Status;
    EFI_FILE_IO_INTERFACE *FileSystem;
    FSW_VOLUME_DATA     *Volume;
    
#if DEBUG_LEVEL
    Print(L"fsw_efi_DriverBinding_Stop\n");
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
    Print(L"fsw_efi_DriverBinding_Stop: protocol uninstalled successfully\n");
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

EFI_STATUS EFIAPI fsw_efi_ComponentName_GetDriverName(IN  EFI_COMPONENT_NAME_PROTOCOL  *This,
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

EFI_STATUS EFIAPI fsw_efi_ComponentName_GetControllerName(IN  EFI_COMPONENT_NAME_PROTOCOL    *This,
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

EFI_STATUS fsw_efi_map_status(fsw_status_t fsw_status, FSW_VOLUME_DATA *Volume)
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

EFI_STATUS EFIAPI fsw_efi_FileSystem_OpenVolume(IN EFI_FILE_IO_INTERFACE *This,
                                                OUT EFI_FILE **Root)
{
    EFI_STATUS          Status;
    FSW_VOLUME_DATA     *Volume = FSW_VOLUME_FROM_FILE_SYSTEM(This);
    
#if DEBUG_LEVEL
    Print(L"fsw_efi_FileSystem_OpenVolume\n");
#endif
    
    Status = fsw_efi_dnode_to_FileHandle(Volume->vol->root, Root);
    
    return Status;
}

//
// EFI File Handle interface (dispatching only)
//

EFI_STATUS EFIAPI fsw_efi_FileHandle_Open(IN EFI_FILE *This,
                                          OUT EFI_FILE **NewHandle,
                                          IN CHAR16 *FileName,
                                          IN UINT64 OpenMode,
                                          IN UINT64 Attributes)
{
    FSW_FILE_DATA      *File = FSW_FILE_FROM_FILE_HANDLE(This);
    
    if (File->Type == FSW_EFI_FILE_TYPE_DIR)
        return fsw_efi_dir_open(File, NewHandle, FileName, OpenMode, Attributes);
    // not supported for regular files
    return EFI_UNSUPPORTED;
}

EFI_STATUS EFIAPI fsw_efi_FileHandle_Close(IN EFI_FILE *This)
{
    FSW_FILE_DATA      *File = FSW_FILE_FROM_FILE_HANDLE(This);
    
#if DEBUG_LEVEL
    Print(L"FswFileClose\n");
#endif
    
    fsw_shandle_close(&File->shand);
    FreePool(File);
    
    return EFI_SUCCESS;
}

EFI_STATUS EFIAPI fsw_efi_FileHandle_Delete(IN EFI_FILE *This)
{
    EFI_STATUS          Status;
    
    Status = This->Close(This);
    if (Status == EFI_SUCCESS) {
        // this driver is read-only
        Status = EFI_WARN_DELETE_FAILURE;
    }
    
    return Status;
}

EFI_STATUS EFIAPI fsw_efi_FileHandle_Read(IN EFI_FILE *This,
                                          IN OUT UINTN *BufferSize,
                                          OUT VOID *Buffer)
{
    FSW_FILE_DATA      *File = FSW_FILE_FROM_FILE_HANDLE(This);
    
    if (File->Type == FSW_EFI_FILE_TYPE_FILE)
        return fsw_efi_file_read(File, BufferSize, Buffer);
    else if (File->Type == FSW_EFI_FILE_TYPE_DIR)
        return fsw_efi_dir_read(File, BufferSize, Buffer);
    return EFI_UNSUPPORTED;
}

EFI_STATUS EFIAPI fsw_efi_FileHandle_Write(IN EFI_FILE *This,
                                           IN OUT UINTN *BufferSize,
                                           IN VOID *Buffer)
{
    // this driver is read-only
    return EFI_WRITE_PROTECTED;
}

EFI_STATUS EFIAPI fsw_efi_FileHandle_GetPosition(IN EFI_FILE *This,
                                                 OUT UINT64 *Position)
{
    FSW_FILE_DATA      *File = FSW_FILE_FROM_FILE_HANDLE(This);
    
    if (File->Type == FSW_EFI_FILE_TYPE_FILE)
        return fsw_efi_file_getpos(File, Position);
    // not defined for directories
    return EFI_UNSUPPORTED;
}

EFI_STATUS EFIAPI fsw_efi_FileHandle_SetPosition(IN EFI_FILE *This,
                                                 IN UINT64 Position)
{
    FSW_FILE_DATA      *File = FSW_FILE_FROM_FILE_HANDLE(This);
    
    if (File->Type == FSW_EFI_FILE_TYPE_FILE)
        return fsw_efi_file_setpos(File, Position);
    else if (File->Type == FSW_EFI_FILE_TYPE_DIR)
        return fsw_efi_dir_setpos(File, Position);
    return EFI_UNSUPPORTED;
}

EFI_STATUS EFIAPI fsw_efi_FileHandle_GetInfo(IN EFI_FILE *This,
                                             IN EFI_GUID *InformationType,
                                             IN OUT UINTN *BufferSize,
                                             OUT VOID *Buffer)
{
    FSW_FILE_DATA      *File = FSW_FILE_FROM_FILE_HANDLE(This);
    
    return fsw_efi_dnode_getinfo(File, InformationType, BufferSize, Buffer);
}

EFI_STATUS EFIAPI fsw_efi_FileHandle_SetInfo(IN EFI_FILE *This,
                                             IN EFI_GUID *InformationType,
                                             IN UINTN BufferSize,
                                             IN VOID *Buffer)
{
    // this driver is read-only
    return EFI_WRITE_PROTECTED;
}

EFI_STATUS EFIAPI fsw_efi_FileHandle_Flush(IN EFI_FILE *This)
{
    // this driver is read-only
    return EFI_WRITE_PROTECTED;
}

//
// Create file handle for a dnode
//

EFI_STATUS fsw_efi_dnode_to_FileHandle(IN struct fsw_dnode *dno,
                                       OUT EFI_FILE **NewFileHandle)
{
    EFI_STATUS          Status;
    FSW_FILE_DATA       *File;
    
    // make sure the dnode has complete info
    Status = fsw_efi_map_status(fsw_dnode_fill(dno), (FSW_VOLUME_DATA *)dno->vol->host_data);
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
    Status = fsw_efi_map_status(fsw_shandle_open(dno, &File->shand),
                                (FSW_VOLUME_DATA *)dno->vol->host_data);
    if (EFI_ERROR(Status)) {
        FreePool(File);
        return Status;
    }
    
    // populate the file handle
    File->FileHandle.Revision    = EFI_FILE_HANDLE_REVISION;
    File->FileHandle.Open        = fsw_efi_FileHandle_Open;
    File->FileHandle.Close       = fsw_efi_FileHandle_Close;
    File->FileHandle.Delete      = fsw_efi_FileHandle_Delete;
    File->FileHandle.Read        = fsw_efi_FileHandle_Read;
    File->FileHandle.Write       = fsw_efi_FileHandle_Write;
    File->FileHandle.GetPosition = fsw_efi_FileHandle_GetPosition;
    File->FileHandle.SetPosition = fsw_efi_FileHandle_SetPosition;
    File->FileHandle.GetInfo     = fsw_efi_FileHandle_GetInfo;
    File->FileHandle.SetInfo     = fsw_efi_FileHandle_SetInfo;
    File->FileHandle.Flush       = fsw_efi_FileHandle_Flush;
    
    *NewFileHandle = &File->FileHandle;
    return EFI_SUCCESS;
}

//
// Functions for regular files
//

EFI_STATUS fsw_efi_file_read(IN FSW_FILE_DATA *File,
                             IN OUT UINTN *BufferSize,
                             OUT VOID *Buffer)
{
    EFI_STATUS          Status;
    fsw_u32             buffer_size;
    
#if DEBUG_LEVEL
    Print(L"fsw_efi_file_read %d bytes\n", *BufferSize);
#endif
    
    buffer_size = *BufferSize;
    Status = fsw_efi_map_status(fsw_shandle_read(&File->shand, &buffer_size, Buffer),
                                (FSW_VOLUME_DATA *)File->shand.dnode->vol->host_data);
    *BufferSize = buffer_size;
    
    return Status;
}

EFI_STATUS fsw_efi_file_getpos(IN FSW_FILE_DATA *File,
                               OUT UINT64 *Position)
{
    *Position = File->shand.pos;
    return EFI_SUCCESS;
}

EFI_STATUS fsw_efi_file_setpos(IN FSW_FILE_DATA *File,
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

EFI_STATUS fsw_efi_dir_open(IN FSW_FILE_DATA *File,
                            OUT EFI_FILE **NewHandle,
                            IN CHAR16 *FileName,
                            IN UINT64 OpenMode,
                            IN UINT64 Attributes)
{
    EFI_STATUS          Status;
    FSW_VOLUME_DATA     *Volume = (FSW_VOLUME_DATA *)File->shand.dnode->vol->host_data;
    struct fsw_dnode    *dno;
    struct fsw_dnode    *target_dno;
    struct fsw_string   lookup_path;
    
#if DEBUG_LEVEL
    Print(L"fsw_efi_dir_open: '%s'\n", FileName);
#endif
    
    if (OpenMode != EFI_FILE_MODE_READ)
        return EFI_WRITE_PROTECTED;
    
    lookup_path.type = FSW_STRING_TYPE_UTF16;
    lookup_path.len  = StrLen(FileName);
    lookup_path.size = lookup_path.len * sizeof(fsw_u16);
    lookup_path.data = FileName;
    
    // resolve the path (symlinks along the way are automatically resolved)
    Status = fsw_efi_map_status(fsw_dnode_lookup_path(File->shand.dnode, &lookup_path, '\\', &dno),
                                Volume);
    if (EFI_ERROR(Status))
        return Status;
    
    // if the final node is a symlink, also resolve it
    Status = fsw_efi_map_status(fsw_dnode_resolve(dno, &target_dno),
                                Volume);
    fsw_dnode_release(dno);
    if (EFI_ERROR(Status))
        return Status;
    dno = target_dno;
    
    // make a new EFI handle for the target dnode
    Status = fsw_efi_dnode_to_FileHandle(dno, NewHandle);
    fsw_dnode_release(dno);
    return Status;
}

//
// EFI_FILE Read for directories
//

EFI_STATUS fsw_efi_dir_read(IN FSW_FILE_DATA *File,
                            IN OUT UINTN *BufferSize,
                            OUT VOID *Buffer)
{
    EFI_STATUS          Status;
    FSW_VOLUME_DATA     *Volume = (FSW_VOLUME_DATA *)File->shand.dnode->vol->host_data;
    struct fsw_dnode    *dno;
    
#if DEBUG_LEVEL
    Print(L"fsw_efi_dir_read...\n");
#endif
    
    // read the next entry
    Status = fsw_efi_map_status(fsw_dnode_dir_read(&File->shand, &dno),
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
    Status = fsw_efi_dnode_fill_FileInfo(Volume, dno, BufferSize, Buffer);
    fsw_dnode_release(dno);
    return Status;
}

//
// EFI_FILE SetPosition for directories
//

EFI_STATUS fsw_efi_dir_setpos(IN FSW_FILE_DATA *File,
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

EFI_STATUS fsw_efi_dnode_getinfo(IN FSW_FILE_DATA *File,
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
        Print(L"fsw_efi_dnode_getinfo: FILE_INFO\n");
#endif
        
        Status = fsw_efi_dnode_fill_FileInfo(Volume, File->shand.dnode, BufferSize, Buffer);
        
    } else if (CompareGuid(InformationType, &FileSystemInfo) == 0) {
#if DEBUG_LEVEL
        Print(L"fsw_efi_dnode_getinfo: FILE_SYSTEM_INFO\n");
#endif
        
        // check buffer size
        RequiredSize = SIZE_OF_EFI_FILE_SYSTEM_INFO + fsw_efi_strsize(&Volume->vol->label);
        if (*BufferSize < RequiredSize) {
            *BufferSize = RequiredSize;
            return EFI_BUFFER_TOO_SMALL;
        }
        
        // fill structure
        FSInfo = (EFI_FILE_SYSTEM_INFO *)Buffer;
        FSInfo->Size        = RequiredSize;
        FSInfo->ReadOnly    = TRUE;
        FSInfo->BlockSize   = Volume->vol->log_blocksize;
        fsw_efi_strcpy(FSInfo->VolumeLabel, &Volume->vol->label);
        
        // get the missing info from the fs driver
        ZeroMem(&vsb, sizeof(struct fsw_volume_stat));
        Status = fsw_efi_map_status(fsw_volume_stat(Volume->vol, &vsb), Volume);
        if (EFI_ERROR(Status))
            return Status;
        FSInfo->VolumeSize  = vsb.total_bytes;
        FSInfo->FreeSpace   = vsb.free_bytes;
        
        // prepare for return
        *BufferSize = RequiredSize;
        Status = EFI_SUCCESS;
        
    } else if (CompareGuid(InformationType, &FileSystemVolumeLabelInfo) == 0) {
#if DEBUG_LEVEL
        Print(L"fsw_efi_dnode_getinfo: FILE_SYSTEM_VOLUME_LABEL\n");
#endif
        
        // check buffer size
        RequiredSize = SIZE_OF_EFI_FILE_SYSTEM_VOLUME_LABEL_INFO + fsw_efi_strsize(&Volume->vol->label);
        if (*BufferSize < RequiredSize) {
            *BufferSize = RequiredSize;
            return EFI_BUFFER_TOO_SMALL;
        }
        
        // copy volume label
        fsw_efi_strcpy(((EFI_FILE_SYSTEM_VOLUME_LABEL_INFO *)Buffer)->VolumeLabel, &Volume->vol->label);
        
        // prepare for return
        *BufferSize = RequiredSize;
        Status = EFI_SUCCESS;
        
    } else {
        Status = EFI_UNSUPPORTED;
    }
    
    return Status;
}

static void fsw_efi_store_time_posix(struct fsw_dnode_stat *sb, int which, fsw_u32 posix_time)
{
    EFI_FILE_INFO       *FileInfo = (EFI_FILE_INFO *)sb->host_data;
    
    if (which == FSW_DNODE_STAT_CTIME)
        fsw_efi_decode_time(&FileInfo->CreateTime,       posix_time);
    else if (which == FSW_DNODE_STAT_MTIME)
        fsw_efi_decode_time(&FileInfo->ModificationTime, posix_time);
    else if (which == FSW_DNODE_STAT_ATIME)
        fsw_efi_decode_time(&FileInfo->LastAccessTime,   posix_time);
}

static void fsw_efi_store_attr_posix(struct fsw_dnode_stat *sb, fsw_u16 posix_mode)
{
    EFI_FILE_INFO       *FileInfo = (EFI_FILE_INFO *)sb->host_data;
    
    if ((posix_mode & S_IWUSR) == 0)
        FileInfo->Attribute |= EFI_FILE_READ_ONLY;
}

EFI_STATUS fsw_efi_dnode_fill_FileInfo(IN FSW_VOLUME_DATA *Volume,
                                       IN struct fsw_dnode *dno,
                                       IN OUT UINTN *BufferSize,
                                       OUT VOID *Buffer)
{
    EFI_STATUS          Status;
    EFI_FILE_INFO       *FileInfo;
    UINTN               RequiredSize;
    struct fsw_dnode_stat sb;
    
    // make sure the dnode has complete info
    Status = fsw_efi_map_status(fsw_dnode_fill(dno), Volume);
    if (EFI_ERROR(Status))
        return Status;
    
    // TODO: check/assert that the dno's name is in UTF16
    
    // check buffer size
    RequiredSize = SIZE_OF_EFI_FILE_INFO + fsw_efi_strsize(&dno->name);
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
    fsw_efi_strcpy(FileInfo->FileName, &dno->name);
    
    // get the missing info from the fs driver
    ZeroMem(&sb, sizeof(struct fsw_dnode_stat));
    sb.store_time_posix = fsw_efi_store_time_posix;
    sb.store_attr_posix = fsw_efi_store_attr_posix;
    sb.host_data = FileInfo;
    Status = fsw_efi_map_status(fsw_dnode_stat(dno, &sb), Volume);
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

// EOF
