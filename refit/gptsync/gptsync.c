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

PARTITION_INFO mbr_parts[4];
UINTN          mbr_part_count = 0;
PARTITION_INFO gpt_parts[128];
UINTN          gpt_part_count = 0;

UINT8 sector[512];


static UINTN read_mbr(VOID)
{
    UINTN status;
    
    /* read MBR data */
    status = read_sector(0, sector);
    if (status != 0)
        return status;
    
    /* check for validity */
    if (sector[510] != 0x55 || sector[511] != 0xAA)
        return 0;  /* no MBR found (really shouldn't happen...) */
    
    /* TODO: check each entry: bootable flag 0x00 or 0x80, start LBA
      and size > 0 */
    
    
    return 0;
}

static UINTN read_gpt(VOID)
{
    UINTN status;
    
    /* read GPT header */
    status = read_sector(1, sector);
    if (status != 0)
        return status;
    
    /* TODO */
}

UINTN gptsync(VOID)
{
    UINTN status;
    
    /* get full information from disk */
    status = read_mbr();
    if (status != 0)
        return status;
    status = read_gpt();
    if (status != 0)
        return status;
    
    /* cross-check current situation */
    
    
    /* offer user the choice what to do */
    
    
    /* adjust the MBR and write it back */
    
    
    return status;
}
