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

struct fs_ext2_inode;

typedef struct {
    UINT64                      Signature;
    
    EFI_FILE_IO_INTERFACE       FileSystem;
    
    EFI_HANDLE                  Handle;
    EFI_DISK_IO                 *DiskIo;
    EFI_BLOCK_IO                *BlockIo;
    UINT32                      MediaId;
    
    struct ext2_super_block     *SuperBlock;
    UINT32                      BlockSize;
    UINT32                      IndBlockCount;
    UINT32                      DIndBlockCount;
    UINT32                      InodeSize;
    struct fs_ext2_inode        *RootInode;
    struct fs_ext2_inode        *DirInodeList;
    
    UINT32                      BlockInBuffer;
    UINT8                       *BlockBuffer;
    
} EXT2_VOLUME_DATA;

#define EXT2_VOLUME_FROM_FILE_SYSTEM(a)  CR (a, EXT2_VOLUME_DATA, FileSystem, EXT2_VOLUME_DATA_SIGNATURE)

typedef struct fs_ext2_inode {
    EXT2_VOLUME_DATA            *Volume;
    UINT32                      InodeNo;
    UINT32                      RefCount;
    struct fs_ext2_inode        *ParentDirInode;
    CHAR16                      *Name;
    
    struct ext2_inode           *RawInode;
    UINT64                      FileSize;
    
    struct fs_ext2_inode        *Next;
    struct fs_ext2_inode        *Prev;
} EXT2_INODE;

typedef struct {
    EXT2_INODE                  *Inode;
    UINT64                      CurrentPosition;
    UINT32                      CurrentFileBlockNo;
    UINT32                      CurrentVolBlockNo;
} EXT2_INODE_HANDLE;

typedef struct {
    UINT64                      Signature;
    
    EFI_FILE                    FileHandle;
    
    EXT2_INODE_HANDLE           InodeHandle;
    UINTN                       Kind;
    
} EXT2_FILE_DATA;

#define EXT2_FILE_KIND_FILE  (0)
#define EXT2_FILE_KIND_DIR   (1)

#define EXT2_FILE_FROM_FILE_HANDLE(a)  CR (a, EXT2_FILE_DATA, FileHandle, EXT2_FILE_DATA_SIGNATURE)

//
// private functions
//

EFI_STATUS Ext2ReadSuper(IN EXT2_VOLUME_DATA *Volume);
EFI_STATUS Ext2ReadBlock(IN EXT2_VOLUME_DATA *Volume, IN UINT32 BlockNo);

EFI_STATUS Ext2InodeOpen(IN EXT2_VOLUME_DATA *Volume,
                         IN UINT32 InodeNo,
                         IN EXT2_INODE *ParentDirInode OPTIONAL,
                         IN struct ext2_dir_entry *DirEntry OPTIONAL,
                         OUT EXT2_INODE **NewInode);
EFI_STATUS Ext2InodeClose(IN EXT2_INODE *Inode);
VOID Ext2InodeFillFileInfo(IN EXT2_INODE *Inode,
                           OUT EFI_FILE_INFO *FileInfo);

EFI_STATUS Ext2InodeHandleOpen(IN EXT2_VOLUME_DATA *Volume,
                               IN UINT32 InodeNo,
                               IN EXT2_INODE *ParentDirInode OPTIONAL,
                               IN struct ext2_dir_entry *DirEntry OPTIONAL,
                               OUT EXT2_INODE_HANDLE *InodeHandle);
EFI_STATUS Ext2InodeHandleReopen(IN EXT2_INODE *Inode,
                                 OUT EXT2_INODE_HANDLE *InodeHandle);
EFI_STATUS Ext2InodeHandleClose(IN EXT2_INODE_HANDLE *InodeHandle);
EFI_STATUS Ext2InodeHandleRead(IN EXT2_INODE_HANDLE *InodeHandle,
                               IN OUT UINTN *BufferSize,
                               OUT VOID *Buffer);

EFI_STATUS Ext2FileFromInodeHandle(IN EXT2_INODE_HANDLE *InodeHandle,
                                   OUT EFI_FILE **NewFileHandle);

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
EFI_STATUS EFIAPI Ext2DirRead(IN EFI_FILE *This,
                              IN OUT UINTN *BufferSize,
                              OUT VOID *Buffer);
EFI_STATUS EFIAPI Ext2DirSetPosition(IN EFI_FILE *This,
                                     IN UINT64 Position);
EFI_STATUS EFIAPI Ext2DirGetPosition(IN EFI_FILE *This,
                                     OUT UINT64 *Position);


#endif
