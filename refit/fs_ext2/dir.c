/*
 * fs_ext2/dir.c
 * EFI interface functions for directories
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

// MAX_LINK_COUNT

EFI_STATUS Ext2DirReadEntry(IN EXT2_VOLUME_DATA *Volume,
                            IN EXT2_INODE *Inode,
                            OUT struct ext2_dir_entry **DirEntry)
{
    EFI_STATUS          Status;
    struct ext2_dir_entry LocalEntry;
    UINTN               BufferSize, EntryLength;
    
    while (1) {
        // read dir_entry header (fixed length)
        BufferSize = 8;
        Status = Ext2InodeRead(Volume, Inode, &BufferSize, &LocalEntry);
        if (EFI_ERROR(Status))
            return Status;
        
        if (BufferSize < 8 || LocalEntry.rec_len == 0) {
            // end of directory reached (or invalid entry)
            *DirEntry = NULL;
            return EFI_SUCCESS;
        }
        if (LocalEntry.rec_len < 8)
            return EFI_VOLUME_CORRUPTED;
        if (LocalEntry.inode != 0) {
            // this entry is used, process it
            break;
        }
        
        // valid, but unused entry, skip it
        Inode->CurrentPosition += LocalEntry.rec_len - 8;
    }
    
    // read file name (variable length)
    BufferSize = LocalEntry.name_len;
    Status = Ext2InodeRead(Volume, Inode, &BufferSize, LocalEntry.name);
    if (EFI_ERROR(Status))
        return Status;
    if (BufferSize < LocalEntry.name_len)
        return EFI_VOLUME_CORRUPTED;
    
    // make a copy
    EntryLength = 8 + LocalEntry.name_len;
    *DirEntry = AllocatePool(EntryLength);
    CopyMem(*DirEntry, &LocalEntry, EntryLength);
    
    Inode->CurrentPosition += LocalEntry.rec_len - EntryLength;  // make sure any padding is skipped, too
    
    return EFI_SUCCESS;
}


//
// EFI_FILE Open for directories
//

EFI_STATUS EFIAPI Ext2DirOpen(IN EFI_FILE *This,
                              OUT EFI_FILE **NewHandle,
                              IN CHAR16 *FileName,
                              IN UINT64 OpenMode,
                              IN UINT64 Attributes)
{
    EFI_STATUS          Status;
    EXT2_FILE_DATA      *File;
    EXT2_VOLUME_DATA    *Volume;
    UINT32              InodeNo;
    EXT2_INODE          DirInode;
    struct ext2_dir_entry *DirEntry;
    CHAR16              *PathElement;
    CHAR16              *PathElementEnd;
    CHAR16              *NextPathElement;
    UINTN               PathElementLength, i;
    BOOLEAN             NamesEqual;
    
    Print(L"Ext2DirOpen: '%s'\n", FileName);
    
    File = EXT2_FILE_FROM_FILE_HANDLE(This);
    Volume = File->Volume;
    
    if (OpenMode != EFI_FILE_MODE_READ)
        return EFI_WRITE_PROTECTED;
    
    PathElement = FileName;
    if (*PathElement == '\\') {
        InodeNo = EXT2_ROOT_INO;
        while (*PathElement == '\\')
            PathElement++;
    } else
        InodeNo = File->Inode.InodeNo;
    DirEntry = NULL;
    
    while (*PathElement != 0) {
        // parse next path element
        PathElementEnd = PathElement;
        while (*PathElementEnd != 0 && *PathElementEnd != '\\')
            PathElementEnd++;
        PathElementLength = PathElementEnd - PathElement;
        NextPathElement = PathElementEnd;
        while (*NextPathElement == '\\')
            NextPathElement++;
        
        // dispose last dir entry (from previous loop iteration)
        if (DirEntry != NULL)
            FreePool(DirEntry);
        
        // check root directory constraints
        if (PathElementLength == 2 && PathElement[0] == '.' && PathElement[1] == '.') {
            if (InodeNo == EXT2_ROOT_INO) {
                // EFI spec: there is no parent for the root
                // NOTE: the EFI shell relies on this!
                return EFI_NOT_FOUND;
            }
        }
        
        // open the directory
        Status = Ext2InodeOpen(Volume, InodeNo, &DirInode);
        if (EFI_ERROR(Status))
            return Status;
        if ((DirInode.RawInode->i_mode & S_IFMT) != S_IFDIR) {
            Ext2InodeClose(&DirInode);
            Print(L"Ext2DirOpen: NOT FOUND (not a directory)\n");
            return EFI_NOT_FOUND;
        }
        
        // scan the directory for the file
        while (1) {
            // read next entry
            Status = Ext2DirReadEntry(Volume, &DirInode, &DirEntry);
            if (EFI_ERROR(Status)) {
                Ext2InodeClose(&DirInode);
                return Status;
            }
            if (DirEntry == NULL) {
                // end of directory reached
                Ext2InodeClose(&DirInode);
                Print(L"Ext2DirOpen: NOT FOUND (no match)\n");
                return EFI_NOT_FOUND;
            }
            
            // compare name
            if (DirEntry->name_len == PathElementLength) {
                NamesEqual = TRUE;
                for (i = 0; i < DirEntry->name_len; i++) {
                    if (DirEntry->name[i] != PathElement[i]) {
                        NamesEqual = FALSE;
                        break;
                    }
                }
                if (NamesEqual)
                    break;
            }
            
            FreePool(DirEntry);
        }
        Ext2InodeClose(&DirInode);
        
        // TODO: resolve symbolic link
        
        InodeNo = DirEntry->inode;
        PathElement = NextPathElement;
        
        Print(L"Ext2DirOpen: matched, inode %d\n", InodeNo);
    }
    
    // open the inode we arrived at
    Status = Ext2FileOpenWithInode(Volume, InodeNo, DirEntry, NewHandle);
    if (EFI_ERROR(Status)) {
        if (DirEntry != NULL)
            FreePool(DirEntry);
        return Status;
    }
    // file object takes ownership of the dir entry
    
    Print(L"Ext2DirOpen: returning\n");
    return Status;
}

//
// EFI_FILE Close for directories
//

EFI_STATUS EFIAPI Ext2DirClose(IN EFI_FILE *This)
{
    EXT2_FILE_DATA      *File;
    
    Print(L"Ext2DirClose\n");
    
    File = EXT2_FILE_FROM_FILE_HANDLE(This);
    
    if (File->DirEntry != NULL)
        FreePool(File->DirEntry);
    Ext2InodeClose(&File->Inode);
    FreePool(File);
    
    return EFI_SUCCESS;
}

//
// EFI_FILE Read for directories
//

EFI_STATUS EFIAPI Ext2DirRead(IN EFI_FILE *This,
                              IN OUT UINTN *BufferSize,
                              OUT VOID *Buffer)
{
    EFI_STATUS          Status;
    EXT2_FILE_DATA      *File;
    UINT64              SavedPosition;
    struct ext2_dir_entry *DirEntry;
    EXT2_INODE          EntryInode;
    EFI_FILE_INFO       *FileInfo;
    CHAR16              *DestNamePtr;
    UINTN               i, RequiredSize;
    
    Print(L"Ext2DirRead...\n");
    
    File = EXT2_FILE_FROM_FILE_HANDLE(This);
    
    // read the next dir_entry
    SavedPosition = File->Inode.CurrentPosition;
    Status = Ext2DirReadEntry(File->Volume, &File->Inode, &DirEntry);
    if (EFI_ERROR(Status))
        return Status;
    if (DirEntry == NULL) {
        // end of directory reached
        *BufferSize = 0;
        Print(L"...no more entries\n");
        return EFI_SUCCESS;
    }
    
    // calculate structure size
    RequiredSize = SIZE_OF_EFI_FILE_INFO + (DirEntry->name_len + 1) * sizeof(CHAR16);
    // check buffer size
    if (*BufferSize < RequiredSize) {
        *BufferSize = RequiredSize;
        
        // push the entry back for now
        File->Inode.CurrentPosition = SavedPosition;
        FreePool(DirEntry);
        Print(L"...BUFFER TOO SMALL\n");
        return EFI_BUFFER_TOO_SMALL;
    }
    
    // read inode for further information
    Status = Ext2InodeOpen(File->Volume, DirEntry->inode, &EntryInode);
    if (EFI_ERROR(Status)) {
        FreePool(DirEntry);
        return Status;
    }
    
    // fill structure
    ZeroMem(Buffer, RequiredSize);
    FileInfo = (EFI_FILE_INFO *)Buffer;
    FileInfo->Size = RequiredSize;
    Ext2InodeFillFileInfo(File->Volume, &EntryInode, FileInfo);
    Ext2InodeClose(&EntryInode);
    
    // copy file name
    DestNamePtr = FileInfo->FileName;
    for (i = 0; i < DirEntry->name_len; i++)
        DestNamePtr[i] = DirEntry->name[i];
    DestNamePtr[i] = 0;
    FreePool(DirEntry);
    
    // prepare for return
    *BufferSize = RequiredSize;
    Print(L"...found '%s'\n", FileInfo->FileName);
    return EFI_SUCCESS;
}

//
// EFI_FILE SetPosition for directories
//

EFI_STATUS EFIAPI Ext2DirSetPosition(IN EFI_FILE *This,
                                     IN UINT64 Position)
{
    EXT2_FILE_DATA      *File;
    
    File = EXT2_FILE_FROM_FILE_HANDLE(This);
    if (Position == 0) {
        File->Inode.CurrentPosition = 0;
        return EFI_SUCCESS;
    } else {
        // directories can only rewind to the start
        return EFI_UNSUPPORTED;
    }
}

//
// EFI_FILE GetPosition for directories
//

EFI_STATUS EFIAPI Ext2DirGetPosition(IN EFI_FILE *This,
                                     OUT UINT64 *Position)
{
    // not defined for directories
    return EFI_UNSUPPORTED;
}
