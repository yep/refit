/*
 * gptsync/gptsync.c
 * Platform-independent code for syncing GPT and MBR
 *
 * Copyright (c) 2006 Christoph Pfisterer
 * All rights reserved.
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

#include "gptsync.h"

// types

typedef struct {
    UINTN   index;
    UINT64  start_lba;
    UINT64  end_lba;
    UINTN   mbr_type;
    UINT8   *gpt_type;
} PARTITION_INFO;

typedef struct {
    UINT8   flags;
    UINT8   start_chs[3];
    UINT8   type;
    UINT8   end_chs[3];
    UINT32  start_lba;
    UINT32  size;
} MBR_PARTITION_INFO;

// variables

PARTITION_INFO  mbr_parts[4];
UINTN           mbr_part_count = 0;
PARTITION_INFO  gpt_parts[128];
UINTN           gpt_part_count = 0;

UINT8           sector[512];

//
// MBR functions
//

static UINTN read_mbr(VOID)
{
    UINTN               status;
    UINTN               i;
    BOOLEAN             used;
    MBR_PARTITION_INFO  *table;
    
    // read MBR data
    status = read_sector(0, sector);
    if (status != 0)
        return status;
    
    // check for validity
    if (*((UINT16 *)(sector + 510)) != 0xaa55) {
        Print(L"No MBR signature detected\n");
        return 0;
    }
    table = (MBR_PARTITION_INFO *)(sector + 446);
    for (i = 0; i < 4; i++) {
        if (table[i].flags != 0x00 && table[i].flags != 0x80) {
            Print(L"Invalid MBR partition table\n");
            return 0;
        }
    }
    
    // check if used
    used = FALSE;
    for (i = 0; i < 4; i++) {
        if (table[i].start_lba > 0 && table[i].size > 0) {
            used = TRUE;
            break;
        }
    }
    if (!used) {
        Print(L"MBR table has no partitions defined\n");
        return 0;
    }
    
    // dump current state & fill internal structures
    Print(L"MBR table contains these partitions:\n");
    Print(L" #  Start LBA    End LBA Type\n");
    for (i = 0; i < 4; i++) {
        if (table[i].start_lba == 0 || table[i].size == 0)
            continue;
        
        mbr_parts[mbr_part_count].index     = i;
        mbr_parts[mbr_part_count].start_lba = (UINT64)table[i].start_lba;
        mbr_parts[mbr_part_count].end_lba   = (UINT64)table[i].start_lba + (UINT64)table[i].size - 1;
        mbr_parts[mbr_part_count].mbr_type  = table[i].type;
        mbr_parts[mbr_part_count].gpt_type  = NULL;
        
        Print(L" %d %10d %10d  %02x\n",
              mbr_parts[mbr_part_count].index + 1,
              mbr_parts[mbr_part_count].start_lba,
              mbr_parts[mbr_part_count].end_lba,
              mbr_parts[mbr_part_count].mbr_type);
        
        mbr_part_count++;
    }
    
    return 0;
}

//
// GPT functions
//

static UINTN read_gpt(VOID)
{
    UINTN status;
    
    // read GPT header
    status = read_sector(1, sector);
    if (status != 0)
        return status;
    
    // TODO
    
    return 0;
}

//
// sync algorithm entry point
//

UINTN gptsync(VOID)
{
    UINTN status;
    
    // get full information from disk
    status = read_gpt();
    if (status != 0)
        return status;
    status = read_mbr();
    if (status != 0)
        return status;
    
    // cross-check current situation
    
    
    // offer user the choice what to do
    
    
    // adjust the MBR and write it back
    
    
    return status;
}
