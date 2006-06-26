/*
 * fs_ext2/fs_ext2.c
 * Entry point and main EFI interface functions
 *
 * Copyright (c) 2006 Christoph Pfisterer
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License
 * as published by the Free Software Foundation; either version 2
 * of the License, or (at your option) any later version.
 *
 * See LICENSE.txt for details about the copyright status of
 * the ext2 file system driver.
 */

#include "fs_ext2.h"

// functions

EFI_STATUS EFIAPI Ext2DriverBindingSupported(IN EFI_DRIVER_BINDING_PROTOCOL  *This,
                                             IN EFI_HANDLE                   ControllerHandle,
                                             IN EFI_DEVICE_PATH_PROTOCOL     *RemainingDevicePath);
EFI_STATUS EFIAPI Ext2DriverBindingStart(IN EFI_DRIVER_BINDING_PROTOCOL  *This,
                                         IN EFI_HANDLE                   ControllerHandle,
                                         IN EFI_DEVICE_PATH_PROTOCOL     *RemainingDevicePath);
EFI_STATUS EFIAPI Ext2DriverBindingStop(IN  EFI_DRIVER_BINDING_PROTOCOL  *This,
                                        IN  EFI_HANDLE                   ControllerHandle,
                                        IN  UINTN                        NumberOfChildren,
                                        IN  EFI_HANDLE                   *ChildHandleBuffer);

EFI_STATUS EFIAPI Ext2ComponentNameGetDriverName(IN  EFI_COMPONENT_NAME_PROTOCOL  *This,
                                                 IN  CHAR8                        *Language,
                                                 OUT CHAR16                       **DriverName);
EFI_STATUS EFIAPI Ext2ComponentNameGetControllerName(IN  EFI_COMPONENT_NAME_PROTOCOL    *This,
                                                     IN  EFI_HANDLE                     ControllerHandle,
                                                     IN  EFI_HANDLE                     ChildHandle  OPTIONAL,
                                                     IN  CHAR8                          *Language,
                                                     OUT CHAR16                         **ControllerName);

// interface structures

EFI_DRIVER_BINDING_PROTOCOL gExt2DriverBinding = {
    Ext2DriverBindingSupported,
    Ext2DriverBindingStart,
    Ext2DriverBindingStop,
    0x10,
    NULL,
    NULL
};

EFI_COMPONENT_NAME_PROTOCOL gExt2ComponentName = {
    Ext2ComponentNameGetDriverName,
    Ext2ComponentNameGetControllerName,
    "eng"
};

//
// Ext2EntryPoint: Image entry point
//
// Installs the Driver Binding and Component Name protocols on the image's handle.
// Mounting a file system is initiated through Driver Binding at the firmware's
// request.
//

EFI_DRIVER_ENTRY_POINT (Ext2EntryPoint)

EFI_STATUS EFIAPI Ext2EntryPoint(IN EFI_HANDLE         ImageHandle,
                                 IN EFI_SYSTEM_TABLE   *SystemTable)
{
    EFI_STATUS  Status;
    
    InitializeLib(ImageHandle, SystemTable);
    
    // complete Driver Binding protocol instance
    gExt2DriverBinding.ImageHandle          = ImageHandle;
    gExt2DriverBinding.DriverBindingHandle  = ImageHandle;
    // install Driver Binding protocol
    Status = BS->InstallProtocolInterface(&gExt2DriverBinding.DriverBindingHandle,
                                          &DriverBindingProtocol,
                                          EFI_NATIVE_INTERFACE,
                                          &gExt2DriverBinding);
    if (EFI_ERROR (Status)) {
        return Status;
    }
    
    // install Component Name protocol
    Status = BS->InstallProtocolInterface(&gExt2DriverBinding.DriverBindingHandle,
                                          &ComponentNameProtocol,
                                          EFI_NATIVE_INTERFACE,
                                          &gExt2ComponentName);
    if (EFI_ERROR (Status)) {
        return Status;
    }
    
    return EFI_SUCCESS;
}

//
// Driver Binding protocol interface functions
//
// Ext2DriverBindingSupported: Check if a "controller" is supported by this driver
//

EFI_STATUS EFIAPI Ext2DriverBindingSupported(IN EFI_DRIVER_BINDING_PROTOCOL  *This,
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
// Ext2DriverBindingStart: Start this driver on the given "controller"
//

EFI_STATUS EFIAPI Ext2DriverBindingStart(IN EFI_DRIVER_BINDING_PROTOCOL  *This,
                                         IN EFI_HANDLE                   ControllerHandle,
                                         IN EFI_DEVICE_PATH_PROTOCOL     *RemainingDevicePath)
{
    EFI_STATUS          Status;
    EFI_BLOCK_IO        *BlockIo;
    EFI_DISK_IO         *DiskIo;
    EXT2_VOLUME_DATA    *Volume;
    
    Print(L"Ext2DriverBindingStart\n");
    
    // open consumed protocols
    Status = BS->OpenProtocol(ControllerHandle,
                              &BlockIoProtocol,
                              (VOID **) &BlockIo,
                              This->DriverBindingHandle,
                              ControllerHandle,
                              EFI_OPEN_PROTOCOL_GET_PROTOCOL);
    if (EFI_ERROR(Status))
        return Status;
    
    Status = BS->OpenProtocol(ControllerHandle,
                              &DiskIoProtocol,
                              (VOID **) &DiskIo,
                              This->DriverBindingHandle,
                              ControllerHandle,
                              EFI_OPEN_PROTOCOL_BY_DRIVER);
    if (EFI_ERROR(Status))
        return Status;
    
    // allocate volume structure
    Volume = AllocateZeroPool(sizeof(EXT2_VOLUME_DATA));
    Volume->Signature = EXT2_VOLUME_DATA_SIGNATURE;
    Volume->Handle    = ControllerHandle;
    Volume->DiskIo    = DiskIo;
    Volume->BlockIo   = BlockIo;
    Volume->MediaId   = BlockIo->Media->MediaId;
    
    // read the superblock
    Status = Ext2ReadSuper(Volume);
    
    if (!EFI_ERROR(Status)) {
        // register the SimpleFileSystem protocol
        Status = BS->InstallProtocolInterface(&Volume->Handle,
                                              &FileSystemProtocol,
                                              EFI_NATIVE_INTERFACE,
                                              &Volume->FileSystem);
    }
    
    // on errors, close the opened protocols
    if (EFI_ERROR(Status)) {
        if (Volume->BlockBuffer != NULL)
            FreePool(Volume->BlockBuffer);
        FreePool(Volume);
        BS->CloseProtocol(ControllerHandle,
                          &DiskIoProtocol,
                          This->DriverBindingHandle,
                          ControllerHandle);
        BS->CloseProtocol(ControllerHandle,
                          &BlockIoProtocol,
                          This->DriverBindingHandle,
                          ControllerHandle);
    }
    
    return Status;
}

//
// Ext2DriverBindingStop: Stop this driver on the given "controller"
//

EFI_STATUS EFIAPI Ext2DriverBindingStop(IN  EFI_DRIVER_BINDING_PROTOCOL  *This,
                                        IN  EFI_HANDLE                   ControllerHandle,
                                        IN  UINTN                        NumberOfChildren,
                                        IN  EFI_HANDLE                   *ChildHandleBuffer)
{
    EFI_STATUS          Status;
    EXT2_VOLUME_DATA    *Volume;
    
    Print(L"Ext2DriverBindingStop\n");
    
    // get private data structure
    Volume = EXT2_VOLUME_FROM_FILE_SYSTEM(This);
    
    // uninstall Simple File System protocol
    Status = BS->UninstallProtocolInterface(&Volume->Handle,
                                            &FileSystemProtocol,
                                            &Volume->FileSystem);
    if (EFI_ERROR(Status))
        return Status;
    
    // release private data structure
    if (Volume->BlockBuffer != NULL)
        FreePool(Volume->BlockBuffer);
    FreePool(Volume);
    
    // close the consumed protocols
    BS->CloseProtocol(ControllerHandle,
                      &DiskIoProtocol,
                      This->DriverBindingHandle,
                      ControllerHandle);
    BS->CloseProtocol(ControllerHandle,
                      &BlockIoProtocol,
                      This->DriverBindingHandle,
                      ControllerHandle);
    
    return EFI_SUCCESS;
}

//
// Component Name protocol interface functions
//

EFI_STATUS EFIAPI Ext2ComponentNameGetDriverName(IN  EFI_COMPONENT_NAME_PROTOCOL  *This,
                                                 IN  CHAR8                        *Language,
                                                 OUT CHAR16                       **DriverName)
{
    if (Language == NULL || DriverName == NULL)
        return EFI_INVALID_PARAMETER;
    
    if (Language[0] == 'e' && Language[1] == 'n' && Language[2] == 'g' && Language[3] == 0) {
        *DriverName = L"Ext2 File System";
        return EFI_SUCCESS;
    }
    return EFI_UNSUPPORTED;
}

EFI_STATUS EFIAPI Ext2ComponentNameGetControllerName(IN  EFI_COMPONENT_NAME_PROTOCOL    *This,
                                                     IN  EFI_HANDLE                     ControllerHandle,
                                                     IN  EFI_HANDLE                     ChildHandle  OPTIONAL,
                                                     IN  CHAR8                          *Language,
                                                     OUT CHAR16                         **ControllerName)
{
    return EFI_UNSUPPORTED;
}
