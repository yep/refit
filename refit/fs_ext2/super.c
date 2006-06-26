/*
 * fs_ext2/super.c
 * Per-volume functions, including SimpleFileSystem interface
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


// functions

EFI_STATUS EFIAPI Ext2OpenVolume(IN EFI_FILE_IO_INTERFACE *This,
                                 OUT EFI_FILE **Root);

//
// read the superblock and setup the Simple File System interface
//

EFI_STATUS Ext2ReadSuper(IN EXT2_VOLUME_DATA *Volume)
{
    EFI_STATUS          Status;
    
    // read the superblock from disk
    Volume->SuperBlock = AllocatePool(sizeof(struct ext2_super_block));
    Status = Volume->DiskIo->ReadDisk(Volume->DiskIo, Volume->MediaId,
                                      BLOCK_SIZE,
                                      sizeof(struct ext2_super_block),
                                      Volume->SuperBlock);
    if (EFI_ERROR(Status)) {
        FreePool(Volume->SuperBlock);
        return Status;
    }
    
    // check the superblock
    if (Volume->SuperBlock->s_magic != EXT2_SUPER_MAGIC ||
        (Volume->SuperBlock->s_rev_level != EXT2_GOOD_OLD_REV &&
         Volume->SuperBlock->s_rev_level != EXT2_DYNAMIC_REV)) {
        FreePool(Volume->SuperBlock);
        return EFI_UNSUPPORTED;
    }
    
    // TODO: check s_feature_incompat (if EXT2_DYNAMIC_REV)
    
    Volume->BlockSize = 1024 << Volume->SuperBlock->s_log_block_size;
    Volume->IndBlockCount = Volume->BlockSize / sizeof(__u32);
    Volume->DIndBlockCount = Volume->IndBlockCount * Volume->IndBlockCount;
    if (Volume->SuperBlock->s_rev_level == EXT2_GOOD_OLD_REV) {
        Volume->InodeSize = EXT2_GOOD_OLD_INODE_SIZE;
#if DEBUG_LEVEL
        Print(L"Ext2ReadSuper: EXT2_GOOD_OLD_REV, %d byte inodes\n", Volume->InodeSize);
#endif
    } else {
        Volume->InodeSize = Volume->SuperBlock->s_inode_size;
#if DEBUG_LEVEL
        Print(L"Ext2ReadSuper: EXT2_DYNAMIC_REV, %d byte inodes\n", Volume->InodeSize);
#endif
    }
    Volume->RootInode = NULL;
    Volume->DirInodeList = NULL;
    
    Volume->BlockBuffer = AllocatePool(Volume->BlockSize);
    Volume->BlockInBuffer = 0;   // NOTE: block 0 is never read through Ext2ReadBlock
    
#if DEBUG_LEVEL
    Print(L"Ext2ReadSuper: successful, BlockSize %d\n", Volume->BlockSize);
#endif
    
    // setup the SimpleFileSystem protocol
    Volume->FileSystem.Revision = EFI_FILE_IO_INTERFACE_REVISION;
    Volume->FileSystem.OpenVolume = Ext2OpenVolume;
    
    return EFI_SUCCESS;
}

//
// Ext2OpenVolume
//

EFI_STATUS EFIAPI Ext2OpenVolume(IN EFI_FILE_IO_INTERFACE *This,
                                 OUT EFI_FILE **Root)
{
    EFI_STATUS          Status;
    EXT2_VOLUME_DATA    *Volume;
    EXT2_INODE_HANDLE   InodeHandle;
    
#if DEBUG_LEVEL
    Print(L"Ext2OpenVolume\n");
#endif
    
    Volume = EXT2_VOLUME_FROM_FILE_SYSTEM(This);
    
    // open the root inode, keep it around until the FS is unmounted
    if (Volume->RootInode == NULL) {
        Status = Ext2InodeOpen(Volume, EXT2_ROOT_INO, NULL, NULL, &Volume->RootInode);
        if (EFI_ERROR(Status))
            return Status;
    }
    
    // create a new inode handle
    Status = Ext2InodeHandleReopen(Volume->RootInode, &InodeHandle);
    if (EFI_ERROR(Status))
        return Status;
    
    // wrap it in a file structure
    Status = Ext2FileFromInodeHandle(&InodeHandle, Root);
    // NOTE: file handle takes ownership of inode handle
    
    return Status;
}

//
// read given block into the volume's block buffer
//

EFI_STATUS Ext2ReadBlock(IN EXT2_VOLUME_DATA *Volume, IN UINT32 BlockNo)
{
    EFI_STATUS          Status;
    
    // check buffer
    if (BlockNo == Volume->BlockInBuffer)
        return EFI_SUCCESS;
    
#if DEBUG_LEVEL == 2
    Print(L"Ext2ReadBlock: %d\n", BlockNo);
#endif
    
    // read from disk
    Status = Volume->DiskIo->ReadDisk(Volume->DiskIo, Volume->MediaId,
                                      (UINT64)BlockNo * Volume->BlockSize,
                                      Volume->BlockSize,
                                      Volume->BlockBuffer);
    if (EFI_ERROR(Status)) {
        Volume->BlockInBuffer = 0;   // NOTE: block 0 is never read
        return Status;
    }
    
    // remember buffer state
    Volume->BlockInBuffer = BlockNo;
    return EFI_SUCCESS;
}
