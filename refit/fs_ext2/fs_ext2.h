/*
 * fs_ext2/fs_ext2.h
 * Global header for the ext2 driver
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

#ifndef _FS_EXT2_H_
#define _FS_EXT2_H_

#include <efi.h>
#include <efilib.h>

#include "ext2_disk.h"

//
// private data structures
//

#define EXT2_VOLUME_DATA_SIGNATURE  EFI_SIGNATURE_32 ('e', 'x', '2', 'V')
#define EXT2_FILE_DATA_SIGNATURE    EFI_SIGNATURE_32 ('e', 'x', '2', 'F')

typedef struct {
    UINT64                      Signature;
    
    EFI_FILE_IO_INTERFACE       FileSystem;
    
    EFI_HANDLE                  Handle;
    EFI_DISK_IO                 *DiskIo;
    EFI_BLOCK_IO                *BlockIo;
    UINT32                      MediaId;
    
    struct ext2_super_block     SuperBlock;
    UINT32                      BlockSize;
    UINT32                      IndBlockCount;
    UINT32                      DIndBlockCount;
    
    UINT32                      BlockInBuffer;
    UINT8                       *BlockBuffer;
    
} EXT2_VOLUME_DATA;

#define EXT2_VOLUME_FROM_FILE_SYSTEM(a)  CR (a, EXT2_VOLUME_DATA, FileSystem, EXT2_VOLUME_DATA_SIGNATURE)

typedef struct {
    UINT32                      InodeNo;
    struct ext2_inode           *RawInode;
    UINT64                      FileSize;
    UINT64                      CurrentPosition;
    UINT32                      CurrentFileBlockNo;
    UINT32                      CurrentVolBlockNo;
} EXT2_INODE;

typedef struct {
    UINT64                      Signature;
    
    EFI_FILE                    FileHandle;
    
    EXT2_VOLUME_DATA            *Volume;
    //CHAR16                      *FileName;
    struct ext2_dir_entry       *DirEntry;
    EXT2_INODE                  Inode;
    
} EXT2_FILE_DATA;

#define EXT2_FILE_FROM_FILE_HANDLE(a)  CR (a, EXT2_FILE_DATA, FileHandle, EXT2_FILE_DATA_SIGNATURE)

//
// private functions
//

EFI_STATUS Ext2ReadSuper(IN EXT2_VOLUME_DATA *Volume);
EFI_STATUS Ext2ReadBlock(IN EXT2_VOLUME_DATA *Volume, IN UINT32 BlockNo);

EFI_STATUS Ext2InodeOpen(IN EXT2_VOLUME_DATA *Volume,
                         IN UINT32 InodeNo,
                         OUT EXT2_INODE *Inode);
EFI_STATUS Ext2InodeRead(IN EXT2_VOLUME_DATA *Volume,
                         IN EXT2_INODE *Inode,
                         IN OUT UINTN *BufferSize,
                         OUT VOID *Buffer);
VOID Ext2InodeFillFileInfo(IN EXT2_VOLUME_DATA *Volume,
                           IN EXT2_INODE *Inode,
                           OUT EFI_FILE_INFO *FileInfo);
EFI_STATUS Ext2InodeClose(IN EXT2_INODE *Inode);



EFI_STATUS Ext2FileOpenWithInode(IN EXT2_VOLUME_DATA *Volume,
                                 IN UINT32 InodeNo,
                                 IN struct ext2_dir_entry *DirEntry OPTIONAL,
                                 OUT EFI_FILE **NewFile);


EFI_STATUS EFIAPI Ext2FileOpen(IN EFI_FILE *This,
                               OUT EFI_FILE **NewHandle,
                               IN CHAR16 *FileName,
                               IN UINT64 OpenMode,
                               IN UINT64 Attributes);
EFI_STATUS EFIAPI Ext2FileClose(IN EFI_FILE *This);
EFI_STATUS EFIAPI Ext2FileDelete(IN EFI_FILE *This);
EFI_STATUS EFIAPI Ext2FileRead(IN EFI_FILE *This,
                               IN OUT UINTN *BufferSize,
                               OUT VOID *Buffer);
EFI_STATUS EFIAPI Ext2FileWrite(IN EFI_FILE *This,
                                IN OUT UINTN *BufferSize,
                                IN VOID *Buffer);
EFI_STATUS EFIAPI Ext2FileSetPosition(IN EFI_FILE *This,
                                      IN UINT64 Position);
EFI_STATUS EFIAPI Ext2FileGetPosition(IN EFI_FILE *This,
                                      OUT UINT64 *Position);
EFI_STATUS EFIAPI Ext2FileGetInfo(IN EFI_FILE *This,
                                  IN EFI_GUID *InformationType,
                                  IN OUT UINTN *BufferSize,
                                  OUT VOID *Buffer);
EFI_STATUS EFIAPI Ext2FileSetInfo(IN EFI_FILE *This,
                                  IN EFI_GUID *InformationType,
                                  IN UINTN BufferSize,
                                  IN VOID *Buffer);
EFI_STATUS EFIAPI Ext2FileFlush(IN EFI_FILE *This);

EFI_STATUS EFIAPI Ext2DirOpen(IN EFI_FILE *This,
                              OUT EFI_FILE **NewHandle,
                              IN CHAR16 *FileName,
                              IN UINT64 OpenMode,
                              IN UINT64 Attributes);
EFI_STATUS EFIAPI Ext2DirClose(IN EFI_FILE *This);
EFI_STATUS EFIAPI Ext2DirRead(IN EFI_FILE *This,
                              IN OUT UINTN *BufferSize,
                              OUT VOID *Buffer);
EFI_STATUS EFIAPI Ext2DirSetPosition(IN EFI_FILE *This,
                                     IN UINT64 Position);
EFI_STATUS EFIAPI Ext2DirGetPosition(IN EFI_FILE *This,
                                     OUT UINT64 *Position);


#endif
