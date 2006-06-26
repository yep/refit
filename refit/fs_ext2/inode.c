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

#define INVALID_BLOCK_NO (0xffffffffUL)


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

EFI_STATUS Ext2InodeMapBlock(IN EXT2_VOLUME_DATA *Volume,
                             IN EXT2_INODE *Inode,
                             IN UINT32 FileBlockNo)
{
    EFI_STATUS          Status;
    UINT32              IndBlockNo;
    
    // try direct block pointers in the inode
    if (FileBlockNo < EXT2_NDIR_BLOCKS) {
        Inode->CurrentVolBlockNo = Inode->RawInode->i_block[FileBlockNo];
        return EFI_SUCCESS;
    }
    FileBlockNo -= EXT2_NDIR_BLOCKS;
    
    // try indirect block
    if (FileBlockNo < Volume->IndBlockCount) {
        // read the indirect block into buffer
        Status = Ext2ReadBlock(Volume, Inode->RawInode->i_block[EXT2_IND_BLOCK]);
        if (EFI_ERROR(Status))
            return Status;
        Inode->CurrentVolBlockNo = ((__u32 *)Volume->BlockBuffer)[FileBlockNo];
        return EFI_SUCCESS;
    }
    FileBlockNo -= Volume->IndBlockCount;
    
    // try double-indirect block
    if (FileBlockNo < Volume->DIndBlockCount) {
        // read the double-indirect block into buffer
        Status = Ext2ReadBlock(Volume, Inode->RawInode->i_block[EXT2_DIND_BLOCK]);
        if (EFI_ERROR(Status))
            return Status;
        IndBlockNo = ((__u32 *)Volume->BlockBuffer)[FileBlockNo / Volume->IndBlockCount];
        
        // read the linked indirect block into buffer
        Status = Ext2ReadBlock(Volume, IndBlockNo);
        if (EFI_ERROR(Status))
            return Status;
        Inode->CurrentVolBlockNo = ((__u32 *)Volume->BlockBuffer)[FileBlockNo % Volume->IndBlockCount];
        return EFI_SUCCESS;
    }
    FileBlockNo -= Volume->DIndBlockCount;
    
    // use the triple-indirect block
    // read the triple-indirect block into buffer
    Status = Ext2ReadBlock(Volume, Inode->RawInode->i_block[EXT2_TIND_BLOCK]);
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
    Inode->CurrentVolBlockNo = ((__u32 *)Volume->BlockBuffer)[FileBlockNo % Volume->IndBlockCount];
    return EFI_SUCCESS;
}

EFI_STATUS Ext2InodeOpen(IN EXT2_VOLUME_DATA *Volume,
                         IN UINT32 InodeNo,
                         OUT EXT2_INODE *Inode)
{
    EFI_STATUS          Status;
    UINT32              GroupNo, GroupDescBlockNo, GroupDescIndex;
    struct ext2_group_desc *GroupDesc;
    UINT32              InodeNoInGroup, InodeBlockNo, InodeIndex;
    
    //Print(L"Ext2InodeOpen: %d\n", InodeNo);
    
    // read the group descripter for the block group the inode belongs to
    GroupNo = (InodeNo - 1) / Volume->SuperBlock.s_inodes_per_group;
    GroupDescBlockNo = (Volume->SuperBlock.s_first_data_block + 1) +
        GroupNo / (Volume->BlockSize / sizeof(struct ext2_group_desc));
    GroupDescIndex = GroupNo % (Volume->BlockSize / sizeof(struct ext2_group_desc));
    Status = Ext2ReadBlock(Volume, GroupDescBlockNo);
    if (EFI_ERROR(Status))
        return Status;
    GroupDesc = ((struct ext2_group_desc *)(Volume->BlockBuffer)) + GroupDescIndex;
    // TODO: in the future, read and keep the bg_inode_table field of all block
    //  groups when mounting the file system (optimization)
    
    // read the inode block for the requested inode
    InodeNoInGroup = (InodeNo - 1) % Volume->SuperBlock.s_inodes_per_group;
    InodeBlockNo = GroupDesc->bg_inode_table +
        InodeNoInGroup / (Volume->BlockSize / sizeof(struct ext2_group_desc));
    InodeIndex = InodeNoInGroup % (Volume->BlockSize / sizeof(struct ext2_group_desc));
    Status = Ext2ReadBlock(Volume, InodeBlockNo);
    if (EFI_ERROR(Status))
        return Status;
    
    // keep a copy of the raw inode structure
    Inode->RawInode = AllocatePool(sizeof(struct ext2_inode));
    CopyMem(Inode->RawInode, ((struct ext2_inode *)(Volume->BlockBuffer)) + InodeIndex, sizeof(struct ext2_inode));
    // TODO: dynamic inode sizes
    
    // initialize state for data reading
    Inode->InodeNo = InodeNo;
    Inode->FileSize = Inode->RawInode->i_size;
    Inode->CurrentPosition = 0;
    Inode->CurrentFileBlockNo = INVALID_BLOCK_NO;
    
    return EFI_SUCCESS;
}

EFI_STATUS Ext2InodeRead(IN EXT2_VOLUME_DATA *Volume,
                         IN EXT2_INODE *Inode,
                         IN OUT UINTN *BufferSize,
                         OUT VOID *Buffer)
{
    EFI_STATUS          Status;
    UINTN               RemLength, CopyLength;
    UINT8               *RemBuffer;
    UINT32              Position;
    UINT32              FileBlockNo;
    
    //Print(L"Ext2InodeRead %d bytes at %ld\n", *BufferSize, Inode->CurrentPosition);
    
    if (Inode->CurrentPosition >= Inode->FileSize) {
        // end of file reached
        *BufferSize = 0;
        return EFI_SUCCESS;
    }
    // NOTE: since ext2 only supports 32 bit file sizes, we can now assume CurrentPosition fits in 32 bits.
    // TODO: check how this code compiles on 64 bit archs
    
    // initialize loop variables
    RemBuffer = Buffer;
    RemLength = *BufferSize;
    Position = (UINT32)Inode->CurrentPosition;
    // constrain read to file size
    if (RemLength >        (Inode->FileSize - Position))
        RemLength = (UINTN)(Inode->FileSize - Position);  // the condition ensures this cast is okay
    
    while (RemLength > 0) {
        // find block number to read in the file's terms
        FileBlockNo = Position / Volume->BlockSize;
        // find corresponding disk block
        if (Inode->CurrentFileBlockNo != FileBlockNo) {
            Status = Ext2InodeMapBlock(Volume, Inode, FileBlockNo);
            if (EFI_ERROR(Status)) {
                Inode->CurrentFileBlockNo = INVALID_BLOCK_NO;
                return Status;
            }
            Inode->CurrentFileBlockNo = FileBlockNo;
        }
        
        // load the data block
        Status = Ext2ReadBlock(Volume, Inode->CurrentVolBlockNo);
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
    *BufferSize = Position - (UINT32)Inode->CurrentPosition;
    Inode->CurrentPosition = Position;
    
    return EFI_SUCCESS;
}

VOID Ext2InodeFillFileInfo(IN EXT2_VOLUME_DATA *Volume,
                           IN EXT2_INODE *Inode,
                           OUT EFI_FILE_INFO *FileInfo)
{
    FileInfo->FileSize          = Inode->FileSize;
    FileInfo->PhysicalSize      = Inode->RawInode->i_blocks * 512;   // very, very strange...
    Ext2DecodeTime(&FileInfo->CreateTime,       Inode->RawInode->i_ctime);
    Ext2DecodeTime(&FileInfo->LastAccessTime,   Inode->RawInode->i_atime);
    Ext2DecodeTime(&FileInfo->ModificationTime, Inode->RawInode->i_mtime);
    FileInfo->Attribute         = 0;
    if ((Inode->RawInode->i_mode & S_IFMT) == S_IFDIR)
        FileInfo->Attribute |= EFI_FILE_DIRECTORY;
    if ((Inode->RawInode->i_mode & 0200) == 0)
        FileInfo->Attribute |= EFI_FILE_READ_ONLY;
}

EFI_STATUS Ext2InodeClose(IN EXT2_INODE *Inode)
{
    //Print(L"Ext2InodeClose: %d\n", Inode->InodeNo);
    FreePool(Inode->RawInode);
    return EFI_SUCCESS;
}
