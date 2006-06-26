/*
 * fs_ext2/file.c
 * EFI interface functions for files
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

EFI_GUID  gEfiFileInfoGuid = EFI_FILE_INFO_ID;
EFI_GUID  gEfiFileSystemInfoGuid = EFI_FILE_SYSTEM_INFO_ID;
EFI_GUID  gEfiFileSystemVolumeLabelInfoIdGuid = EFI_FILE_SYSTEM_VOLUME_LABEL_INFO_ID;


//
// open a file or directory given a inode number
//

EFI_STATUS Ext2FileOpenWithInode(IN EXT2_VOLUME_DATA *Volume,
                                 IN UINT32 InodeNo,
                                 IN struct ext2_dir_entry *DirEntry OPTIONAL,
                                 OUT EFI_FILE **NewFileHandle)
{
    EFI_STATUS          Status;
    EXT2_FILE_DATA      *File;
    
    // allocate file structure
    File = AllocateZeroPool(sizeof(EXT2_FILE_DATA));
    File->Signature = EXT2_FILE_DATA_SIGNATURE;
    File->Volume    = Volume;
    File->DirEntry  = DirEntry;
    
    // open the inode
    Status = Ext2InodeOpen(Volume, InodeNo, &File->Inode);
    if (EFI_ERROR(Status)) {
        FreePool(File);
        return Status;
    }
    
    // check the type
    if ((File->Inode.RawInode->i_mode & S_IFMT) == S_IFREG) {
        // regular file
        File->FileHandle.Revision    = EFI_FILE_HANDLE_REVISION;
        File->FileHandle.Open        = Ext2FileOpen;
        File->FileHandle.Close       = Ext2FileClose;
        File->FileHandle.Delete      = Ext2FileDelete;
        File->FileHandle.Read        = Ext2FileRead;
        File->FileHandle.Write       = Ext2FileWrite;
        File->FileHandle.GetPosition = Ext2FileGetPosition;
        File->FileHandle.SetPosition = Ext2FileSetPosition;
        File->FileHandle.GetInfo     = Ext2FileGetInfo;
        File->FileHandle.SetInfo     = Ext2FileSetInfo;
        File->FileHandle.Flush       = Ext2FileFlush;
        Status = EFI_SUCCESS;
        
    } else if ((File->Inode.RawInode->i_mode & S_IFMT) == S_IFDIR) {
        // directory
        File->FileHandle.Revision    = EFI_FILE_HANDLE_REVISION;
        File->FileHandle.Open        = Ext2DirOpen;
        File->FileHandle.Close       = Ext2DirClose;
        File->FileHandle.Delete      = Ext2FileDelete;
        File->FileHandle.Read        = Ext2DirRead;
        File->FileHandle.Write       = Ext2FileWrite;
        File->FileHandle.GetPosition = Ext2DirGetPosition;
        File->FileHandle.SetPosition = Ext2DirSetPosition;
        File->FileHandle.GetInfo     = Ext2FileGetInfo;
        File->FileHandle.SetInfo     = Ext2FileSetInfo;
        File->FileHandle.Flush       = Ext2FileFlush;
        Status = EFI_SUCCESS;
        
    } else if ((File->Inode.RawInode->i_mode & S_IFMT) == S_IFLNK) {
        // symbolic link
        Status = EFI_UNSUPPORTED;
        // TODO: read the target, look it up, recurse
        
    } else {
        // some kind of special file
        Status = EFI_UNSUPPORTED;
    }
    
    if (EFI_ERROR(Status)) {
        Ext2InodeClose(&File->Inode);
        FreePool(File);
    } else {
        *NewFileHandle = &File->FileHandle;
    }
    return Status;
}


//
// EFI_FILE Open for files
//

EFI_STATUS EFIAPI Ext2FileOpen(IN EFI_FILE *This,
                               OUT EFI_FILE **NewHandle,
                               IN CHAR16 *FileName,
                               IN UINT64 OpenMode,
                               IN UINT64 Attributes)
{
    // only supported for directories...
    return EFI_UNSUPPORTED;
}

//
// EFI_FILE Close for files
//

EFI_STATUS EFIAPI Ext2FileClose(IN EFI_FILE *This)
{
    EXT2_FILE_DATA      *File;
    
    Print(L"Ext2FileClose\n");
    
    File = EXT2_FILE_FROM_FILE_HANDLE(This);
    
    if (File->DirEntry != NULL)
        FreePool(File->DirEntry);
    Ext2InodeClose(&File->Inode);
    FreePool(File);
    
    return EFI_SUCCESS;
}

//
// EFI_FILE Delete for files and directories
//

EFI_STATUS EFIAPI Ext2FileDelete(IN EFI_FILE *This)
{
    EFI_STATUS Status;
    
    Status = This->Close(This);  // use the appropriate close method
    if (Status == EFI_SUCCESS) {
        // this driver is read-only
        Status = EFI_WARN_DELETE_FAILURE;
    }
    
    return Status;
}

//
// EFI_FILE Read for files
//

EFI_STATUS EFIAPI Ext2FileRead(IN EFI_FILE *This,
                               IN OUT UINTN *BufferSize,
                               OUT VOID *Buffer)
{
    EFI_STATUS          Status;
    EXT2_FILE_DATA      *File;
    
    Print(L"Ext2FileRead %d bytes\n", *BufferSize);
    
    File = EXT2_FILE_FROM_FILE_HANDLE(This);
    
    Status = Ext2InodeRead(File->Volume, &File->Inode, BufferSize, Buffer);
    
    return Status;
}

//
// EFI_FILE Write for files and directories
//

EFI_STATUS EFIAPI Ext2FileWrite(IN EFI_FILE *This,
                                IN OUT UINTN *BufferSize,
                                IN VOID *Buffer)
{
    // this driver is read-only
    return EFI_WRITE_PROTECTED;
}

//
// EFI_FILE SetPosition for files
//

EFI_STATUS EFIAPI Ext2FileSetPosition(IN EFI_FILE *This,
                                      IN UINT64 Position)
{
    EXT2_FILE_DATA      *File;
    
    File = EXT2_FILE_FROM_FILE_HANDLE(This);
    
    if (Position == 0xFFFFFFFFFFFFFFFFULL)
        File->Inode.CurrentPosition = File->Inode.FileSize;
    else
        File->Inode.CurrentPosition = Position;
    
    return EFI_SUCCESS;
}

//
// EFI_FILE GetPosition for files
//

EFI_STATUS EFIAPI Ext2FileGetPosition(IN EFI_FILE *This,
                                      OUT UINT64 *Position)
{
    EXT2_FILE_DATA      *File;
    
    File = EXT2_FILE_FROM_FILE_HANDLE(This);
    
    *Position = File->Inode.CurrentPosition;
    
    return EFI_SUCCESS;
}

//
// EFI_FILE GetInfo for files and directories
//

EFI_STATUS EFIAPI Ext2FileGetInfo(IN EFI_FILE *This,
                                  IN EFI_GUID *InformationType,
                                  IN OUT UINTN *BufferSize,
                                  OUT VOID *Buffer)
{
    EFI_STATUS          Status;
    EXT2_FILE_DATA      *File;
    EXT2_VOLUME_DATA    *Volume;
    EFI_FILE_INFO       *FileInfo;
    EFI_FILE_SYSTEM_INFO *FSInfo;
    CHAR8               *NamePtr;
    CHAR16              *DestNamePtr;
    UINTN               i, RequiredSize;
    
    File = EXT2_FILE_FROM_FILE_HANDLE(This);
    Volume = File->Volume;
    
    if (CompareGuid(InformationType, &gEfiFileInfoGuid) == 0) {
        Print(L"Ext2FileGetInfo: FILE_INFO\n");
        
        // get file name size, derive structure size
        if (File->DirEntry == NULL)
            RequiredSize = SIZE_OF_EFI_FILE_INFO + sizeof(CHAR16);
        else
            RequiredSize = SIZE_OF_EFI_FILE_INFO + (File->DirEntry->name_len + 1) * sizeof(CHAR16);
        
        // check buffer size
        if (*BufferSize < RequiredSize) {
            *BufferSize = RequiredSize;
            return EFI_BUFFER_TOO_SMALL;
        }
        
        // fill structure
        FileInfo = (EFI_FILE_INFO *)Buffer;
        FileInfo->Size = RequiredSize;
        Ext2InodeFillFileInfo(Volume, &File->Inode, FileInfo);
        
        // copy file name
        DestNamePtr = FileInfo->FileName;
        if (File->DirEntry == NULL) {
            DestNamePtr[0] = 0;
        } else {
            for (i = 0; i < File->DirEntry->name_len; i++)
                DestNamePtr[i] = File->DirEntry->name[i];
            DestNamePtr[i] = 0;
        }
        
        // prepare for return
        *BufferSize = RequiredSize;
        Status = EFI_SUCCESS;
        
    } else if (CompareGuid(InformationType, &gEfiFileSystemInfoGuid) == 0) {
        Print(L"Ext2FileGetInfo: FILE_SYSTEM_INFO\n");
        
        // get volume label size, derive structure size
        NamePtr = (CHAR8 *)(&Volume->SuperBlock) + 120;
        for (i = 0; i < 16; i++)
            if (NamePtr[i] == 0)
                break;
        RequiredSize = SIZE_OF_EFI_FILE_SYSTEM_INFO + (i + 1) * sizeof(CHAR16);
        
        // check buffer size
        if (*BufferSize < RequiredSize) {
            *BufferSize = RequiredSize;
            return EFI_BUFFER_TOO_SMALL;
        }
        
        // fill structure
        FSInfo = (EFI_FILE_SYSTEM_INFO *)Buffer;
        FSInfo->Size        = RequiredSize;
        FSInfo->ReadOnly    = TRUE;
        FSInfo->VolumeSize  = (UINT64)Volume->SuperBlock.s_blocks_count * Volume->BlockSize;
        FSInfo->FreeSpace   = (UINT64)Volume->SuperBlock.s_free_blocks_count * Volume->BlockSize;
        FSInfo->BlockSize   = Volume->BlockSize;
        
        // copy volume label
        DestNamePtr = FSInfo->VolumeLabel;
        for (i = 0; i < 16; i++) {
            if (NamePtr[i] == 0)
                break;
            DestNamePtr[i] = NamePtr[i];
        }
        DestNamePtr[i] = 0;
        
        // prepare for return
        *BufferSize = RequiredSize;
        Status = EFI_SUCCESS;
        
    } else if (CompareGuid(InformationType, &gEfiFileSystemVolumeLabelInfoIdGuid) == 0) {
        Print(L"Ext2FileGetInfo: FILE_SYSTEM_VOLUME_LABEL\n");
        
        // get volume label size, derive structure size
        NamePtr = (CHAR8 *)(&Volume->SuperBlock) + 120;
        for (i = 0; i < 16; i++)
            if (NamePtr[i] == 0)
                break;
        RequiredSize = SIZE_OF_EFI_FILE_SYSTEM_VOLUME_LABEL_INFO + (i + 1) * sizeof(CHAR16);
        
        // check buffer size
        if (*BufferSize < RequiredSize) {
            *BufferSize = RequiredSize;
            return EFI_BUFFER_TOO_SMALL;
        }
        
        // copy volume label
        DestNamePtr = ((EFI_FILE_SYSTEM_VOLUME_LABEL_INFO *)Buffer)->VolumeLabel;
        for (i = 0; i < 16; i++) {
            if (NamePtr[i] == 0)
                break;
            DestNamePtr[i] = NamePtr[i];
        }
        DestNamePtr[i] = 0;
        
        // prepare for return
        *BufferSize = RequiredSize;
        Status = EFI_SUCCESS;
        
    } else {
        Status = EFI_UNSUPPORTED;
    }
    
    return Status;
}

//
// EFI_FILE SetInfo for files and directories
//

EFI_STATUS EFIAPI Ext2FileSetInfo(IN EFI_FILE *This,
                                  IN EFI_GUID *InformationType,
                                  IN UINTN BufferSize,
                                  IN VOID *Buffer)
{
    // this driver is read-only
    return EFI_WRITE_PROTECTED;
}

//
// EFI_FILE Flush for files and directories
//

EFI_STATUS EFIAPI Ext2FileFlush(IN EFI_FILE *This)
{
    // this driver is read-only
    return EFI_WRITE_PROTECTED;
}
