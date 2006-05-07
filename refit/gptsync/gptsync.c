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
    UINT8   gpt_type[16];
} PARTITION_INFO;

typedef struct {
    UINT8   flags;
    UINT8   start_chs[3];
    UINT8   type;
    UINT8   end_chs[3];
    UINT32  start_lba;
    UINT32  size;
} MBR_PARTITION_INFO;

typedef struct {
    UINT8   type;
    CHARN   *name;
} MBR_PARTTYPE;

typedef struct {
    UINT64  signature;
    UINT32  spec_revision;
    UINT32  header_size;
    UINT32  header_crc32;
    UINT32  reserved;
    UINT64  header_lba;
    UINT64  alternate_header_lba;
    UINT64  first_usable_lba;
    UINT64  last_usable_lba;
    UINT8   disk_guid[16];
    UINT64  entry_lba;
    UINT32  entry_count;
    UINT32  entry_size;
    UINT32  entry_crc32;
} GPT_HEADER;

typedef struct {
    UINT8   type_guid[16];
    UINT8   partition_guid[16];
    UINT64  start_lba;
    UINT64  end_lba;
    UINT64  attributes;
    CHAR16  name[36];
} GPT_ENTRY;

typedef struct {
    UINT8   guid[16];
    UINT8   mbr_type;
    CHARN   *name;
} GPT_PARTTYPE;

// variables

UINT8           empty_guid[16] = { 0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0 };

PARTITION_INFO  mbr_parts[4];
UINTN           mbr_part_count = 0;
PARTITION_INFO  gpt_parts[128];
UINTN           gpt_part_count = 0;

UINT8           sector[512];

MBR_PARTTYPE    mbr_types[] = {
    { 0x01, STR("FAT12") },
    { 0x04, STR("FAT16 <32M") },
    { 0x05, STR("DOS Extended") },
    { 0x06, STR("FAT16") },
    { 0x07, STR("NTFS") },
    { 0x0b, STR("FAT32") },
    { 0x0c, STR("FAT32 (LBA)") },
    { 0x0e, STR("FAT16 (LBA)") },
    { 0x0f, STR("Win95 Extended (LBA)") },
    { 0x11, STR("Hidden FAT12") },
    { 0x14, STR("Hidden FAT16 <32M") },
    { 0x16, STR("Hidden FAT16") },
    { 0x17, STR("Hidden NTFS") },
    { 0x1b, STR("Hidden FAT32") },
    { 0x1c, STR("Hidden FAT32 (LBA)") },
    { 0x1e, STR("Hidden FAT16 (LBA)") },
    { 0x82, STR("Linux swap / Solaris") },
    { 0x83, STR("Linux") },
    { 0x85, STR("Linux Extended") },
    { 0x86, STR("NTFS volume set") },
    { 0x87, STR("NTFS volume set") },
    { 0x8e, STR("Linux LVM") },
    { 0xa5, STR("FreeBSD") },
    { 0xa6, STR("OpenBSD") },
    { 0xa7, STR("NeXTSTEP") },
    { 0xa9, STR("NetBSD") },
    { 0xaf, STR("Mac OS X HFS+") },
    { 0xeb, STR("BeOS") },
    { 0xee, STR("EFI Protective") },
    { 0xef, STR("EFI System (FAT)") },
    { 0xfd, STR("Linux RAID") },
    { 0, NULL },
};

GPT_PARTTYPE    gpt_types[] = {
    { "\x28\x73\x2A\xC1\x1F\xF8\xD2\x11\xBA\x4B\x00\xA0\xC9\x3E\xC9\x3B", 0xef, STR("EFI System (FAT)") },
    { "\x41\xEE\x4D\x02\xE7\x33\xD3\x11\x9D\x69\x00\x08\xC7\x81\xF3\x9F", 0x00, STR("MBR partition scheme") },
    { "\x16\xE3\xC9\xE3\x5C\x0B\xB8\x4D\x81\x7D\xF9\x2D\xF0\x02\x15\xAE", 0x00, STR("MS Reserved") },
    { "\xA2\xA0\xD0\xEB\xE5\xB9\x33\x44\x87\xC0\x68\xB6\xB7\x26\x99\xC7", 0x00, STR("Basic Data") },
    { "\xAA\xC8\x08\x58\x8F\x7E\xE0\x42\x85\xD2\xE1\xE9\x04\x34\xCF\xB3", 0x00, STR("MS LDM Metadata") },
    { "\xA0\x60\x9B\xAF\x31\x14\x62\x4F\xBC\x68\x33\x11\x71\x4A\x69\xAD", 0x00, STR("MS LDM Data") },
    { "\x0F\x88\x9D\xA1\xFC\x05\x3B\x4D\xA0\x06\x74\x3F\x0F\x84\x91\x1E", 0xfd, STR("Linux RAID") },
    { "\x6D\xFD\x57\x06\xAB\xA4\xC4\x43\x84\xE5\x09\x33\xC8\x4B\x4F\x4F", 0x82, STR("Linux Swap") },
    { "\x79\xD3\xD6\xE6\x07\xF5\xC2\x44\xA2\x3C\x23\x8F\x2A\x3D\xF9\x28", 0x8e, STR("Linux LVM") },
    { "\x39\x33\xA6\x8D\x07\x00\xC0\x60\xC4\x36\x08\x3A\xC8\x23\x09\x08", 0x00, STR("Linux Reserved") },
    { "\x00\x53\x46\x48\x00\x00\xAA\x11\xAA\x11\x00\x30\x65\x43\xEC\xAC", 0xaf, STR("Mac OS X HFS+") },
    { { 0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0 }, 0, NULL },
};

//
// MBR functions
//

static CHARN * mbr_parttype_name(UINT8 type)
{
    int i;
    
    for (i = 0; mbr_types[i].name; i++)
        if (mbr_types[i].type == type)
            return mbr_types[i].name;
    return STR("Unknown");
}

static UINTN read_mbr(VOID)
{
    UINTN               status;
    UINTN               i;
    BOOLEAN             used;
    MBR_PARTITION_INFO  *table;
    
    Print(L"Current MBR partition table:\n");
    
    // read MBR data
    status = read_sector(0, sector);
    if (status != 0)
        return status;
    
    // check for validity
    if (*((UINT16 *)(sector + 510)) != 0xaa55) {
        Print(L" No MBR partition table present!\n");
        return 0;
    }
    table = (MBR_PARTITION_INFO *)(sector + 446);
    for (i = 0; i < 4; i++) {
        if (table[i].flags != 0x00 && table[i].flags != 0x80) {
            Print(L" MBR partition table is invalid!\n");
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
        Print(L" No partitions defined\n");
        return 0;
    }
    
    // dump current state & fill internal structures
    Print(L" #    Start LBA      End LBA  Type\n");
    for (i = 0; i < 4; i++) {
        if (table[i].start_lba == 0 || table[i].size == 0)
            continue;
        
        mbr_parts[mbr_part_count].index     = i;
        mbr_parts[mbr_part_count].start_lba = (UINT64)table[i].start_lba;
        mbr_parts[mbr_part_count].end_lba   = (UINT64)table[i].start_lba + (UINT64)table[i].size - 1;
        mbr_parts[mbr_part_count].mbr_type  = table[i].type;
        copy_guid(mbr_parts[mbr_part_count].gpt_type, empty_guid);
        
        Print(L" %d %12lld %12lld  %02x  %s\n",
              mbr_parts[mbr_part_count].index + 1,
              mbr_parts[mbr_part_count].start_lba,
              mbr_parts[mbr_part_count].end_lba,
              mbr_parts[mbr_part_count].mbr_type,
              mbr_parttype_name(mbr_parts[mbr_part_count].mbr_type));
        
        mbr_part_count++;
    }
    
    return 0;
}

//
// GPT functions
//

static CHARN * gpt_parttype_name(UINT8 *type_guid)
{
    int i;
    
    for (i = 0; gpt_types[i].name; i++)
        if (guids_are_equal(gpt_types[i].guid, type_guid))
            return gpt_types[i].name;
    return STR("Unknown");
}

static UINTN read_gpt(VOID)
{
    UINTN       status;
    GPT_HEADER  *header;
    GPT_ENTRY   *entry;
    UINT64      entry_lba;
    UINTN       entry_count, entry_size, i;
    
    Print(L"Current GPT partition table:\n");
    
    // read GPT header
    status = read_sector(1, sector);
    if (status != 0)
        return status;
    
    // check signature
    header = (GPT_HEADER *)sector;
    if (header->signature != 0x5452415020494645ULL) {
        Print(L" No GPT partition table present!\n");
        return 0;
    }
    if (header->spec_revision != 0x00010000UL) {
        Print(L" Warning: Unknown GPT spec revision 0x%08x\n", header->spec_revision);
    }
    if ((512 % header->entry_size) > 0 || header->entry_size > 512) {
        Print(L" Error: Invalid GPT entry size (misaligned or more than 512 bytes)\n");
        return 0;
    }
    
    // read entries
    entry_lba   = header->entry_lba;
    entry_size  = header->entry_size;
    entry_count = header->entry_count;
    
    Print(L" #    Start LBA      End LBA  Type\n");
    for (i = 0; i < entry_count; i++) {
        if (((i * entry_size) % 512) == 0) {
            status = read_sector(entry_lba, sector);
            if (status != 0)
                return status;
            entry_lba++;
        }
        entry = (GPT_ENTRY *)(sector + ((i * entry_size) % 512));
        
        if (guids_are_equal(entry->type_guid, empty_guid))
            continue;
        
        gpt_parts[gpt_part_count].index     = i;
        gpt_parts[gpt_part_count].start_lba = entry->start_lba;
        gpt_parts[gpt_part_count].end_lba   = entry->end_lba;
        gpt_parts[gpt_part_count].mbr_type  = 0;   // TODO
        copy_guid(gpt_parts[gpt_part_count].gpt_type, entry->type_guid);
        
        Print(L" %d %12lld %12lld  %s\n",
              gpt_parts[gpt_part_count].index + 1,
              gpt_parts[gpt_part_count].start_lba,
              gpt_parts[gpt_part_count].end_lba,
              gpt_parttype_name(gpt_parts[gpt_part_count].gpt_type));
        
        gpt_part_count++;
    }
    
    return 0;
}

//
// sync algorithm entry point
//

UINTN gptsync(VOID)
{
    UINTN   status = 0;
    UINTN   status_gpt, status_mbr;
    
    // get full information from disk
    status_gpt = read_gpt();
    status_mbr = read_mbr();
    if (status_gpt != 0 || status_mbr != 0)
        return (status_gpt || status_mbr);
    
    // cross-check current situation
    
    
    // offer user the choice what to do
    
    
    // adjust the MBR and write it back
    
    
    return status;
}
