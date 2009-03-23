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

VOID Ext2DecodeTime(OUT EFI_TIME *EfiTime, IN UINT32 UnixTime)
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
