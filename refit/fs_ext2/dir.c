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

#define DEBUG_LEVEL 0


// MAX_LINK_COUNT

EFI_STATUS Ext2DirReadEntry(IN EXT2_INODE_HANDLE *InodeHandle,
                            OUT struct ext2_dir_entry *DirEntry)
{
    EFI_STATUS          Status;
    UINTN               BufferSize;
    
    while (1) {
        // read dir_entry header (fixed length)
        BufferSize = 8;
        Status = Ext2InodeHandleRead(InodeHandle, &BufferSize, DirEntry);
        if (EFI_ERROR(Status))
            return Status;
        
        if (BufferSize < 8 || DirEntry->rec_len == 0) {
            // end of directory reached (or invalid entry)
            DirEntry->inode = 0;
            return EFI_SUCCESS;
        }
        if (DirEntry->rec_len < 8)
            return EFI_VOLUME_CORRUPTED;
        if (DirEntry->inode != 0) {
            // this entry is used
            if (DirEntry->rec_len < 8 + DirEntry->name_len)
                return EFI_VOLUME_CORRUPTED;
            break;
        }
        
        // valid, but unused entry, skip it
        InodeHandle->CurrentPosition += DirEntry->rec_len - 8;
    }
    
    // read file name (variable length)
    BufferSize = DirEntry->name_len;
    Status = Ext2InodeHandleRead(InodeHandle, &BufferSize, DirEntry->name);
    if (EFI_ERROR(Status))
        return Status;
    if (BufferSize < DirEntry->name_len)
        return EFI_VOLUME_CORRUPTED;
    
    // skip any remaining padding
    InodeHandle->CurrentPosition += DirEntry->rec_len - (8 + DirEntry->name_len);
    
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
    EXT2_INODE          *BaseDirInode;
    EXT2_INODE_HANDLE   CurrentInodeHandle;
    EXT2_INODE_HANDLE   NextInodeHandle;
    struct ext2_dir_entry DirEntry;
    UINT32              NextInodeNo;
    CHAR16              *PathElement;
    CHAR16              *PathElementEnd;
    CHAR16              *NextPathElement;
    UINTN               PathElementLength, i;
    BOOLEAN             NamesEqual;
    
#if DEBUG_LEVEL
    Print(L"Ext2DirOpen: '%s'\n", FileName);
#endif
    
    File = EXT2_FILE_FROM_FILE_HANDLE(This);
    Volume = File->InodeHandle.Inode->Volume;
    
    if (OpenMode != EFI_FILE_MODE_READ)
        return EFI_WRITE_PROTECTED;
    
    // analyze start of path, pick starting point
    PathElement = FileName;
    if (*PathElement == '\\') {
        BaseDirInode = Volume->RootInode;
        while (*PathElement == '\\')
            PathElement++;
    } else
        BaseDirInode = File->InodeHandle.Inode;
    
    // open inode for directory reading
    Status = Ext2InodeHandleReopen(BaseDirInode, &CurrentInodeHandle);
    if (EFI_ERROR(Status))
        return Status;
    
    // loop over the path
    // loop invariant: CurrentInodeHandle is an open, rewinded handle to the current inode
    for (; *PathElement != 0; PathElement = NextPathElement) {
        // parse next path element
        PathElementEnd = PathElement;
        while (*PathElementEnd != 0 && *PathElementEnd != '\\')
            PathElementEnd++;
        PathElementLength = PathElementEnd - PathElement;
        NextPathElement = PathElementEnd;
        while (*NextPathElement == '\\')
            NextPathElement++;
        
        // check that this actually is a directory
        if (!S_ISDIR(CurrentInodeHandle.Inode->RawInode->i_mode)) {
#if DEBUG_LEVEL == 2
            Print(L"Ext2DirOpen: NOT FOUND (not a directory)\n");
#endif
            Status = EFI_NOT_FOUND;
            goto bailout;
        }
        
        // check for . and ..
        NextInodeNo = 0;
        if (PathElementLength == 1 && PathElement[0] == '.') {
            NextInodeNo = CurrentInodeHandle.Inode->InodeNo;
        } else if (PathElementLength == 2 && PathElement[0] == '.' && PathElement[1] == '.') {
            if (CurrentInodeHandle.Inode->ParentDirInode == NULL) {
                // EFI spec says: there is no parent for the root
                // NOTE: the EFI shell relies on this!
                
                Status = EFI_NOT_FOUND;
                goto bailout;
            }
            NextInodeNo = CurrentInodeHandle.Inode->ParentDirInode->InodeNo;
        }
        
        // scan the directory for the file
        while (NextInodeNo == 0) {
            // read next entry
            Status = Ext2DirReadEntry(&CurrentInodeHandle, &DirEntry);
            if (EFI_ERROR(Status))
                goto bailout;
            if (DirEntry.inode == 0) {
                // end of directory reached
#if DEBUG_LEVEL == 2
                Print(L"Ext2DirOpen: NOT FOUND (no match)\n");
#endif
                Status = EFI_NOT_FOUND;
                goto bailout;
            }
            
            // compare name
            if (DirEntry.name_len == PathElementLength) {
                NamesEqual = TRUE;
                for (i = 0; i < DirEntry.name_len; i++) {
                    if (DirEntry.name[i] != PathElement[i]) {
                        NamesEqual = FALSE;
                        break;
                    }
                }
                if (NamesEqual)
                    NextInodeNo = DirEntry.inode;
            }
        }
        
#if DEBUG_LEVEL == 2
        Print(L"Ext2DirOpen: found inode %d\n", NextInodeNo);
#endif
        
        // open the inode we found in the directory
        Status = Ext2InodeHandleOpen(Volume, NextInodeNo, CurrentInodeHandle.Inode, &DirEntry, &NextInodeHandle);
        if (EFI_ERROR(Status))
            goto bailout;
        
        // TODO: resolve symbolic links somehow
        
        // close the previous inode handle and replace it with the new one
        Status = Ext2InodeHandleClose(&CurrentInodeHandle);
        CopyMem(&CurrentInodeHandle, &NextInodeHandle, sizeof(EXT2_INODE_HANDLE));
    }
    
    // wrap the current inode into a file handle
    Status = Ext2FileFromInodeHandle(&CurrentInodeHandle, NewHandle);
    if (EFI_ERROR(Status))
        return Status;
    // NOTE: file handle takes ownership of inode handle
    
#if DEBUG_LEVEL == 2
    Print(L"Ext2DirOpen: returning\n");
#endif
    return Status;
    
bailout:
    Ext2InodeHandleClose(&CurrentInodeHandle);
    return Status;
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
    EXT2_VOLUME_DATA    *Volume;
    UINT64              SavedPosition;
    struct ext2_dir_entry DirEntry;
    EXT2_INODE          *EntryInode;
    EFI_FILE_INFO       *FileInfo;
    UINTN               RequiredSize;
    
#if DEBUG_LEVEL
    Print(L"Ext2DirRead...\n");
#endif
    
    File = EXT2_FILE_FROM_FILE_HANDLE(This);
    Volume = File->InodeHandle.Inode->Volume;
    
    // read the next dir_entry
    SavedPosition = File->InodeHandle.CurrentPosition;
    while (1) {
        Status = Ext2DirReadEntry(&File->InodeHandle, &DirEntry);
        if (EFI_ERROR(Status)) {
            File->InodeHandle.CurrentPosition = SavedPosition;
            return Status;
        }
        if (DirEntry.inode == 0) {
            // end of directory reached
            *BufferSize = 0;
#if DEBUG_LEVEL
            Print(L"...no more entries\n");
#endif
            return EFI_SUCCESS;
        }
        
        // filter out . and ..
        if ((DirEntry.name_len == 1 && DirEntry.name[0] == '.') ||
            (DirEntry.name_len == 2 && DirEntry.name[0] == '.' && DirEntry.name[1] == '.'))
            continue;
        break;
    }
    
    // open inode for the directory entry
    Status = Ext2InodeOpen(Volume, DirEntry.inode, File->InodeHandle.Inode, &DirEntry, &EntryInode);
    if (EFI_ERROR(Status))
        return Status;
    
    // calculate structure size
    RequiredSize = SIZE_OF_EFI_FILE_INFO + StrSize(EntryInode->Name);
    
    // check buffer size
    if (*BufferSize < RequiredSize) {
        // push the entry back for now
        Ext2InodeClose(EntryInode);
        File->InodeHandle.CurrentPosition = SavedPosition;
        
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
    Ext2InodeFillFileInfo(EntryInode, FileInfo);
    StrCpy(FileInfo->FileName, EntryInode->Name);
    Ext2InodeClose(EntryInode);
    
    // prepare for return
    *BufferSize = RequiredSize;
#if DEBUG_LEVEL
    Print(L"...found '%s'\n", FileInfo->FileName);
#endif
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
        File->InodeHandle.CurrentPosition = 0;
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
