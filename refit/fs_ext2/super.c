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
    Status = Volume->DiskIo->ReadDisk(Volume->DiskIo, Volume->MediaId,
                                      BLOCK_SIZE,
                                      sizeof(struct ext2_super_block),
                                      &Volume->SuperBlock);
    if (EFI_ERROR(Status))
        return Status;
    
    // check the superblock
    if (Volume->SuperBlock.s_magic != EXT2_SUPER_MAGIC)
        return EFI_UNSUPPORTED;
    
    Volume->BlockSize = 1024 << Volume->SuperBlock.s_log_block_size;
    Volume->IndBlockCount = Volume->BlockSize / sizeof(__u32);
    Volume->DIndBlockCount = Volume->IndBlockCount * Volume->IndBlockCount;
    Volume->BlockBuffer = AllocatePool(Volume->BlockSize);
    Volume->BlockInBuffer = 0;
    
    Print(L"Ext2ReadSuper: successful, BlockSize %d\n", Volume->BlockSize);
    
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
    
    Print(L"Ext2OpenVolume\n");
    
    Volume = EXT2_VOLUME_FROM_FILE_SYSTEM(This);
    
    Status = Ext2FileOpenWithInode(Volume, EXT2_ROOT_INO, NULL, Root);
    
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
    
    //Print(L"Ext2ReadBlock: %d\n", BlockNo);
    
    // read from disk
    Status = Volume->DiskIo->ReadDisk(Volume->DiskIo, Volume->MediaId,
                                      BlockNo * Volume->BlockSize,
                                      Volume->BlockSize,
                                      Volume->BlockBuffer);
    if (EFI_ERROR(Status)) {
        Volume->BlockInBuffer = 0;
        return Status;
    }
    
    // remember buffer state
    Volume->BlockInBuffer = BlockNo;
    return EFI_SUCCESS;
}
