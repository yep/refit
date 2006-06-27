/*
 * fs_ext2/inode.c
 * Shared functions for inode handling
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


VOID Ext2DecodeTime(OUT EFI_TIME *EfiTime, IN UINT32 UnixTime)
{
    ZeroMem(EfiTime, sizeof(EFI_TIME));
    
    /* TODO: actually map the unix date to the EFI_TIME structure:
     typedef struct {          
         UINT16      Year;       // 1998 - 20XX
         UINT8       Month;      // 1 - 12
         UINT8       Day;        // 1 - 31
         UINT8       Hour;       // 0 - 23
         UINT8       Minute;     // 0 - 59
         UINT8       Second;     // 0 - 59
         UINT8       Pad1;
         UINT32      Nanosecond; // 0 - 999,999,999
         INT16       TimeZone;   // -1440 to 1440 or 2047
         UINT8       Daylight;
         UINT8       Pad2;
     } EFI_TIME;
     */
}

//
// base inode functions
//

EFI_STATUS Ext2InodeOpen(IN EXT2_VOLUME_DATA *Volume,
                         IN UINT32 InodeNo,
                         IN EXT2_INODE *ParentDirInode OPTIONAL,
                         IN struct ext2_dir_entry *DirEntry OPTIONAL,
                         OUT EXT2_INODE **NewInode)
{
    EFI_STATUS          Status;
    EXT2_INODE          *Inode;
    UINT32              GroupNo, GroupDescBlockNo, GroupDescIndex;
    struct ext2_group_desc *GroupDesc;
    UINT32              InodeNoInGroup, InodeBlockNo, InodeIndex;
    UINTN               i;
    
#if DEBUG_LEVEL
    Print(L"Ext2InodeOpen: %d\n", InodeNo);
#endif
    
    // first check the volume's list of open directory inodes
    for (Inode = Volume->DirInodeList; Inode != NULL; Inode = Inode->Next) {
        if (Inode->InodeNo == InodeNo) {
            Inode->RefCount++;
            *NewInode = Inode;
#if DEBUG_LEVEL
            Print(L"...found dir %d '%s', now %d refs\n", Inode->InodeNo, Inode->Name, Inode->RefCount);
#endif
            return EFI_SUCCESS;
        }
    }
    
    // read the group descripter for the block group the inode belongs to
    GroupNo = (InodeNo - 1) / Volume->SuperBlock->s_inodes_per_group;
    GroupDescBlockNo = (Volume->SuperBlock->s_first_data_block + 1) +
        GroupNo / (Volume->BlockSize / sizeof(struct ext2_group_desc));
    GroupDescIndex = GroupNo % (Volume->BlockSize / sizeof(struct ext2_group_desc));
    Status = Ext2ReadBlock(Volume, GroupDescBlockNo);
    if (EFI_ERROR(Status))
        return Status;
    GroupDesc = ((struct ext2_group_desc *)(Volume->BlockBuffer)) + GroupDescIndex;
    // TODO: in the future, read and keep the bg_inode_table field of all block
    //  groups when mounting the file system (optimization)
    
    // read the inode block for the requested inode
    InodeNoInGroup = (InodeNo - 1) % Volume->SuperBlock->s_inodes_per_group;
    InodeBlockNo = GroupDesc->bg_inode_table +
        InodeNoInGroup / (Volume->BlockSize / Volume->InodeSize);
    InodeIndex = InodeNoInGroup % (Volume->BlockSize / Volume->InodeSize);
    Status = Ext2ReadBlock(Volume, InodeBlockNo);
    if (EFI_ERROR(Status))
        return Status;
    
    // set up the inode structure
    Inode = AllocateZeroPool(sizeof(EXT2_INODE));
    Inode->Volume = Volume;
    Inode->InodeNo = InodeNo;
    Inode->RefCount = 1;
    if (ParentDirInode != NULL) {
        ParentDirInode->RefCount++;
        Inode->ParentDirInode = ParentDirInode;
#if DEBUG_LEVEL
        Print(L"...parent inode %d '%s', %d refs\n", ParentDirInode->InodeNo,
              ParentDirInode->Name, ParentDirInode->RefCount);
#endif
    }
    
    // convert file name
    if (DirEntry != NULL) {
        Inode->Name = AllocatePool((DirEntry->name_len + 1) * sizeof(CHAR16));
        for (i = 0; i < DirEntry->name_len; i++)
            Inode->Name[i] = DirEntry->name[i];
        Inode->Name[i] = 0;
    } else
        Inode->Name = StrDuplicate(L"");
    
    // keep the raw inode structure around
    Inode->RawInode = AllocatePool(Volume->InodeSize);
    CopyMem(Inode->RawInode, Volume->BlockBuffer + InodeIndex * Volume->InodeSize, Volume->InodeSize);
    
    Inode->FileSize = Inode->RawInode->i_size;
    // TODO: check docs for 64-bit sized files
    
    if (S_ISDIR(Inode->RawInode->i_mode)) {
        // add to the volume's list of open directories
        if (Volume->DirInodeList != NULL) {
            Volume->DirInodeList->Prev = Inode;
            Inode->Next = Volume->DirInodeList;
        }
        Volume->DirInodeList = Inode;
    }
    
    *NewInode = Inode;
#if DEBUG_LEVEL
    Print(L"...created inode %d '%s', %d refs\n", Inode->InodeNo, Inode->Name, Inode->RefCount);
#endif
    return EFI_SUCCESS;
}

EFI_STATUS Ext2InodeClose(IN EXT2_INODE *Inode)
{
#if DEBUG_LEVEL
    Print(L"Ext2InodeClose: %d '%s', %d refs\n", Inode->InodeNo, Inode->Name, Inode->RefCount);
#endif
    
    Inode->RefCount--;
    if (Inode->RefCount == 0) {
        if (Inode->ParentDirInode != NULL)
            Ext2InodeClose(Inode->ParentDirInode);
        
        if (S_ISDIR(Inode->RawInode->i_mode)) {
            // remove from the volume's list of open directories
            if (Inode->Next)
                Inode->Next->Prev = Inode->Prev;
            if (Inode->Prev)
                Inode->Prev->Next = Inode->Next;
            if (Inode->Volume->DirInodeList == Inode)
                Inode->Volume->DirInodeList = Inode->Next;
        }
        
        FreePool(Inode->RawInode);
        FreePool(Inode->Name);
        FreePool(Inode);
    }
    
    return EFI_SUCCESS;
}

VOID Ext2InodeFillFileInfo(IN EXT2_INODE *Inode,
                           OUT EFI_FILE_INFO *FileInfo)
{
    FileInfo->FileSize          = Inode->FileSize;
    FileInfo->PhysicalSize      = Inode->RawInode->i_blocks * 512;   // very, very strange...
    Ext2DecodeTime(&FileInfo->CreateTime,       Inode->RawInode->i_ctime);
    Ext2DecodeTime(&FileInfo->LastAccessTime,   Inode->RawInode->i_atime);
    Ext2DecodeTime(&FileInfo->ModificationTime, Inode->RawInode->i_mtime);
    FileInfo->Attribute         = 0;
    if (S_ISDIR(Inode->RawInode->i_mode))
        FileInfo->Attribute |= EFI_FILE_DIRECTORY;
    if ((Inode->RawInode->i_mode & S_IWUSR) == 0)
        FileInfo->Attribute |= EFI_FILE_READ_ONLY;
}

//
// inode handle functions
//

EFI_STATUS Ext2InodeHandleOpen(IN EXT2_VOLUME_DATA *Volume,
                               IN UINT32 InodeNo,
                               IN EXT2_INODE *ParentDirInode OPTIONAL,
                               IN struct ext2_dir_entry *DirEntry OPTIONAL,
                               OUT EXT2_INODE_HANDLE *InodeHandle)
{
    EFI_STATUS          Status;
    
    // open the actual inode
    Status = Ext2InodeOpen(Volume, InodeNo, ParentDirInode, DirEntry, &InodeHandle->Inode);
    if (EFI_ERROR(Status))
        return Status;
    
    InodeHandle->CurrentPosition = 0;
    InodeHandle->CurrentFileBlockNo = INVALID_BLOCK_NO;
    
    return EFI_SUCCESS;
}

EFI_STATUS Ext2InodeHandleReopen(IN EXT2_INODE *Inode,
                                 OUT EXT2_INODE_HANDLE *InodeHandle)
{
    InodeHandle->Inode = Inode;
    InodeHandle->CurrentPosition = 0;
    InodeHandle->CurrentFileBlockNo = INVALID_BLOCK_NO;
    
    Inode->RefCount++;
    
#if DEBUG_LEVEL
    Print(L"Ext2InodeHandleReopen: %d '%s', now %d refs\n", Inode->InodeNo, Inode->Name, Inode->RefCount);
#endif
    
    return EFI_SUCCESS;
}

EFI_STATUS Ext2InodeHandleClose(IN EXT2_INODE_HANDLE *InodeHandle)
{
    Ext2InodeClose(InodeHandle->Inode);
    InodeHandle->Inode = NULL;   // just for safety
    
    return EFI_SUCCESS;
}

EFI_STATUS Ext2InodeHandleMapBlock(IN EXT2_INODE_HANDLE *InodeHandle,
                                   IN UINT32 FileBlockNo)
{
    EFI_STATUS          Status;
    EXT2_VOLUME_DATA    *Volume;
    UINT32              IndBlockNo;
    
    Volume = InodeHandle->Inode->Volume;
    
    // try direct block pointers in the inode
    if (FileBlockNo < EXT2_NDIR_BLOCKS) {
        InodeHandle->CurrentVolBlockNo = InodeHandle->Inode->RawInode->i_block[FileBlockNo];
        return EFI_SUCCESS;
    }
    FileBlockNo -= EXT2_NDIR_BLOCKS;
    
    // try indirect block
    if (FileBlockNo < Volume->IndBlockCount) {
        // read the indirect block into buffer
        Status = Ext2ReadBlock(Volume, InodeHandle->Inode->RawInode->i_block[EXT2_IND_BLOCK]);
        if (EFI_ERROR(Status))
            return Status;
        InodeHandle->CurrentVolBlockNo = ((__u32 *)Volume->BlockBuffer)[FileBlockNo];
        return EFI_SUCCESS;
    }
    FileBlockNo -= Volume->IndBlockCount;
    
    // try double-indirect block
    if (FileBlockNo < Volume->DIndBlockCount) {
        // read the double-indirect block into buffer
        Status = Ext2ReadBlock(Volume, InodeHandle->Inode->RawInode->i_block[EXT2_DIND_BLOCK]);
        if (EFI_ERROR(Status))
            return Status;
        IndBlockNo = ((__u32 *)Volume->BlockBuffer)[FileBlockNo / Volume->IndBlockCount];
        
        // read the linked indirect block into buffer
        Status = Ext2ReadBlock(Volume, IndBlockNo);
        if (EFI_ERROR(Status))
            return Status;
        InodeHandle->CurrentVolBlockNo = ((__u32 *)Volume->BlockBuffer)[FileBlockNo % Volume->IndBlockCount];
        return EFI_SUCCESS;
    }
    FileBlockNo -= Volume->DIndBlockCount;
    
    // use the triple-indirect block
    // read the triple-indirect block into buffer
    Status = Ext2ReadBlock(Volume, InodeHandle->Inode->RawInode->i_block[EXT2_TIND_BLOCK]);
    if (EFI_ERROR(Status))
        return Status;
    IndBlockNo = ((__u32 *)Volume->BlockBuffer)[FileBlockNo / Volume->DIndBlockCount];
    
    // read the linked double-indirect block into buffer
    Status = Ext2ReadBlock(Volume, IndBlockNo);
    if (EFI_ERROR(Status))
        return Status;
    IndBlockNo = ((__u32 *)Volume->BlockBuffer)[(FileBlockNo / Volume->IndBlockCount) % Volume->IndBlockCount];
    
    // read the linked indirect block into buffer
    Status = Ext2ReadBlock(Volume, IndBlockNo);
    if (EFI_ERROR(Status))
        return Status;
    InodeHandle->CurrentVolBlockNo = ((__u32 *)Volume->BlockBuffer)[FileBlockNo % Volume->IndBlockCount];
    return EFI_SUCCESS;
}

EFI_STATUS Ext2InodeHandleRead(IN EXT2_INODE_HANDLE *InodeHandle,
                               IN OUT UINTN *BufferSize,
                               OUT VOID *Buffer)
{
    EFI_STATUS          Status;
    EXT2_VOLUME_DATA    *Volume;
    UINTN               RemLength, CopyLength;
    UINT8               *RemBuffer;
    UINT32              Position;
    UINT32              FileBlockNo;
    
    //Print(L"Ext2InodeHandleRead %d bytes at %ld\n", *BufferSize, InodeHandle->CurrentPosition);
    
    Volume = InodeHandle->Inode->Volume;
    
    if (InodeHandle->CurrentPosition >= InodeHandle->Inode->FileSize) {
        // end of file reached
        *BufferSize = 0;
        return EFI_SUCCESS;
    }
    // NOTE: since ext2 only supports 32 bit file sizes, we can now assume CurrentPosition fits in 32 bits.
    // TODO: check how this code compiles on 64 bit archs
    
    // initialize loop variables
    RemBuffer = Buffer;
    RemLength = *BufferSize;
    Position = (UINT32)InodeHandle->CurrentPosition;
    // constrain read to file size
    if (RemLength >        (InodeHandle->Inode->FileSize - Position))
        RemLength = (UINTN)(InodeHandle->Inode->FileSize - Position);  // the condition ensures this cast is okay
    
    while (RemLength > 0) {
        // find block number to read in the file's terms
        FileBlockNo = Position / Volume->BlockSize;
        // find corresponding disk block
        if (InodeHandle->CurrentFileBlockNo != FileBlockNo) {
            Status = Ext2InodeHandleMapBlock(InodeHandle, FileBlockNo);
            if (EFI_ERROR(Status)) {
                InodeHandle->CurrentFileBlockNo = INVALID_BLOCK_NO;
                return Status;
            }
            InodeHandle->CurrentFileBlockNo = FileBlockNo;
        }
        
        // load the data block
        Status = Ext2ReadBlock(Volume, InodeHandle->CurrentVolBlockNo);
        if (EFI_ERROR(Status))
            return Status;
        
        // copy data to the buffer
        CopyLength = Volume->BlockSize - (Position & (Volume->BlockSize - 1));
        if (CopyLength > RemLength)
            CopyLength = RemLength;
        CopyMem(RemBuffer, Volume->BlockBuffer + (Position & (Volume->BlockSize - 1)), CopyLength);
        
        // advance loop variables
        RemBuffer += CopyLength;
        RemLength -= CopyLength;
        Position  += CopyLength;
    }
    
    // calculate bytes actually read
    *BufferSize = Position - (UINT32)InodeHandle->CurrentPosition;
    InodeHandle->CurrentPosition = Position;
    
    return EFI_SUCCESS;
}
