/*
 * fsw/fsw_ext2.h
 * Header with ext2-specific definitions
 *
 * Copyright (c) 2006 Christoph Pfisterer
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License
 * as published by the Free Software Foundation; either version 2
 * of the License, or (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 59 Temple Place - Suite 330, Boston, MA  02111-1307, USA.
 */

#ifndef _FSW_EXT2_H_
#define _FSW_EXT2_H_

#define VOLSTRUCTNAME fsw_ext2_volume
#define DNODESTRUCTNAME fsw_ext2_dnode
#include "fsw_core.h"

#include "fsw_ext2_disk.h"


#define EXT2_SUPERBLOCK_BLOCKSIZE  1024
#define EXT2_SUPERBLOCK_BLOCKNO       1


/**
 * ext2: Volume structure with ext2-specific data.
 */

struct fsw_ext2_volume {
    struct fsw_volume g;
    
    struct ext2_super_block *sb;    // full superblock structure
    fsw_u32     ind_bcnt;           // number of blocks addressable through an indirect block
    fsw_u32     dind_bcnt;          // number of blocks addressable through a double-indirect block
    fsw_u32     inode_size;         // size of inode structure in bytes
};

/**
 * ext2: dnode structure with ext2-specific data.
 */

struct fsw_ext2_dnode {
    struct fsw_dnode g;
    
    struct ext2_inode *raw;         // full inode structure
};


#endif
