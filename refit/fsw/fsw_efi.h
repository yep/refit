/**
 * \file fsw_efi.h
 * EFI host environment header.
 */

/*-
 * Copyright (c) 2006 Christoph Pfisterer
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions are
 * met:
 *
 *  * Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 *
 *  * Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the
 *    distribution.
 *
 *  * Neither the name of Christoph Pfisterer nor the names of the
 *    contributors may be used to endorse or promote products derived
 *    from this software without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
 * "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
 * LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR
 * A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT
 * OWNER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
 * SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT
 * LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
 * DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
 * THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 * OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#ifndef _FSW_EFI_H_
#define _FSW_EFI_H_

#include "fsw_efi_base.h"
#include "fsw_core.h"


#define INVALID_BLOCK_NO (0xffffffffUL)

//
// private data structures
//

#define FSW_VOLUME_DATA_SIGNATURE  EFI_SIGNATURE_32 ('f', 's', 'w', 'V')
#define FSW_FILE_DATA_SIGNATURE    EFI_SIGNATURE_32 ('f', 's', 'w', 'F')

/**
 * EFI Host: Private per-volume structure.
 */

typedef struct {
    UINT64                      Signature;
    
    EFI_FILE_IO_INTERFACE       FileSystem;
    
    EFI_HANDLE                  Handle;
    EFI_DISK_IO                 *DiskIo;
    UINT32                      MediaId;
    EFI_STATUS                  LastIOStatus;
    
    struct fsw_volume           *vol;
    
    fsw_u32                     BlockInBuffer;
    void                        *BlockBuffer;
    
} FSW_VOLUME_DATA;

#define FSW_VOLUME_FROM_FILE_SYSTEM(a)  CR (a, FSW_VOLUME_DATA, FileSystem, FSW_VOLUME_DATA_SIGNATURE)

/**
 * EFI Host: Private structure for a EFI_FILE interface.
 */

typedef struct {
    UINT64                      Signature;
    
    EFI_FILE                    FileHandle;
    
    UINTN                       Type;
    struct fsw_shandle          shand;
    
} FSW_FILE_DATA;

#define FSW_EFI_FILE_TYPE_FILE  (0)
#define FSW_EFI_FILE_TYPE_DIR   (1)

#define FSW_FILE_FROM_FILE_HANDLE(a)  CR (a, FSW_FILE_DATA, FileHandle, FSW_FILE_DATA_SIGNATURE)


//
// Library functions
//

VOID fsw_efi_decode_time(OUT EFI_TIME *EfiTime, IN UINT32 UnixTime);

UINTN fsw_efi_strsize(struct fsw_string *s);
VOID fsw_efi_strcpy(CHAR16 *Dest, struct fsw_string *src);


#endif
