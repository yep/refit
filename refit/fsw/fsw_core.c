/**
 * \file fsw_core.c
 * Core file system wrapper abstraction layer code.
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

#include "fsw_core.h"

#define DEBUG_LEVEL 0


//
// Volume functions
//

fsw_status_t fsw_mount(void *host_data,
                       struct fsw_host_table *host_table,
                       struct fsw_fstype_table *fstype_table,
                       struct fsw_volume **vol_out)
{
    fsw_status_t    status;
    struct fsw_volume *vol;
    
    // allocate memory for the structure
    status = fsw_alloc(fstype_table->volume_struct_size, &vol);
    if (status)
        return status;
    fsw_memzero(vol, fstype_table->volume_struct_size);
    
    // initialize fields
    vol->phys_blocksize = 512;
    vol->log_blocksize  = 512;
    vol->label.type     = FSW_STRING_TYPE_EMPTY;
    vol->host_data      = host_data;
    vol->host_table     = host_table;
    vol->fstype_table   = fstype_table;
    vol->host_string_type = host_table->native_string_type;
    
    // let the fs driver mount the file system
    status = vol->fstype_table->volume_mount(vol);
    if (status)
        goto errorexit;
    
    // TODO: anything else?
    
    *vol_out = vol;
    return FSW_SUCCESS;
    
errorexit:
    fsw_unmount(vol);
    return status;
}

void fsw_unmount(struct fsw_volume *vol)
{
    if (vol->root)
        fsw_dnode_release(vol->root);
    // TODO: check that no other dnodes are still around
    
    vol->fstype_table->volume_free(vol);
    
    fsw_strfree(&vol->label);
    fsw_free(vol);
}

fsw_status_t fsw_volume_stat(struct fsw_volume *vol, struct fsw_volume_stat *sb)
{
    return vol->fstype_table->volume_stat(vol, sb);
}

void fsw_set_blocksize(struct fsw_volume *vol, fsw_u32 phys_blocksize, fsw_u32 log_blocksize)
{
    // TODO: Check the sizes. Both must be powers of 2. log_blocksize must not be smaller than
    //  phys_blocksize.
    
    // signal host driver to drop caches etc.
    vol->host_table->change_blocksize(vol,
                                      vol->phys_blocksize, vol->log_blocksize,
                                      phys_blocksize, log_blocksize);
    
    vol->phys_blocksize = phys_blocksize;
    vol->log_blocksize = log_blocksize;
}

fsw_status_t fsw_read_block(struct fsw_volume *vol, fsw_u32 phys_bno, void **buffer_out)
{
    return vol->host_table->read_block(vol, phys_bno, buffer_out);
}

//
// dnode functions
//

static void fsw_dnode_register(struct fsw_volume *vol, struct fsw_dnode *dno)
{
    dno->next = vol->dnode_head;
    vol->dnode_head->prev = dno;
    dno->prev = NULL;
    vol->dnode_head = dno;
}

fsw_status_t fsw_dnode_create_root(struct fsw_volume *vol, fsw_u32 dnode_id, struct fsw_dnode **dno_out)
{
    fsw_status_t    status;
    struct fsw_dnode *dno;
    
    // allocate memory for the structure
    status = fsw_alloc(vol->fstype_table->dnode_struct_size, &dno);
    if (status)
        return status;
    fsw_memzero(dno, vol->fstype_table->dnode_struct_size);
    
    // fill the structure
    dno->vol = vol;
    dno->parent = NULL;
    dno->dnode_id = dnode_id;
    dno->type = FSW_DNODE_TYPE_DIR;
    dno->refcount = 1;
    dno->name.type = FSW_STRING_TYPE_EMPTY;
    // TODO: instead, call a function to create an empty string in the native string type
    
    fsw_dnode_register(vol, dno);
    
    *dno_out = dno;
    return FSW_SUCCESS;
}

fsw_status_t fsw_dnode_create(struct fsw_dnode *parent_dno, fsw_u32 dnode_id, int type,
                              struct fsw_string *name, struct fsw_dnode **dno_out)
{
    fsw_status_t    status;
    struct fsw_volume *vol = parent_dno->vol;
    struct fsw_dnode *dno;
    
    // check if we already have a dnode with the same id
    for (dno = vol->dnode_head; dno; dno = dno->next) {
        if (dno->dnode_id == dnode_id) {
            dno->refcount++;
            *dno_out = dno;
            return FSW_SUCCESS;
        }
    }
    
    // allocate memory for the structure
    status = fsw_alloc(vol->fstype_table->dnode_struct_size, &dno);
    if (status)
        return status;
    fsw_memzero(dno, vol->fstype_table->dnode_struct_size);
    
    // fill the structure
    dno->vol = vol;
    dno->parent = parent_dno;
    fsw_dnode_retain(dno->parent);
    dno->dnode_id = dnode_id;
    dno->type = type;
    dno->refcount = 1;
    status = fsw_strdup_coerce(&dno->name, vol->host_table->native_string_type, name);
    if (status) {
        fsw_free(dno);
        return status;
    }
    
    fsw_dnode_register(vol, dno);
    
    *dno_out = dno;
    return FSW_SUCCESS;
}

void fsw_dnode_retain(struct fsw_dnode *dno)
{
    dno->refcount++;
}

void fsw_dnode_release(struct fsw_dnode *dno)
{
    struct fsw_volume *vol = dno->vol;
    struct fsw_dnode *parent_dno;
    
    dno->refcount--;
    
    if (dno->refcount == 0) {
        parent_dno = dno->parent;
        
        // de-register from volume's list
        if (dno->next)
            dno->next->prev = dno->prev;
        if (dno->prev)
            dno->prev->next = dno->next;
        if (vol->dnode_head == dno)
            vol->dnode_head = dno->next;
        
        // run fstype-specific cleanup
        vol->fstype_table->dnode_free(vol, dno);
        
        fsw_strfree(&dno->name);
        fsw_free(dno);
        
        // release our pointer to the parent, possibly deallocating it, too
        if (parent_dno)
            fsw_dnode_release(parent_dno);
    }
}

fsw_status_t fsw_dnode_fill(struct fsw_dnode *dno)
{
    return dno->vol->fstype_table->dnode_fill(dno->vol, dno);
}

fsw_status_t fsw_dnode_stat(struct fsw_dnode *dno, struct fsw_dnode_stat *sb)
{
    fsw_status_t    status;
    
    status = fsw_dnode_fill(dno);
    if (status)
        return status;
    
    sb->used_bytes = 0;
    status = dno->vol->fstype_table->dnode_stat(dno->vol, dno, sb);
    if (!status && !sb->used_bytes)
        sb->used_bytes = DivU64x32(dno->size + dno->vol->log_blocksize - 1, dno->vol->log_blocksize, NULL);
    return status;
}

fsw_status_t fsw_dnode_lookup(struct fsw_dnode *dno,
                              struct fsw_string *lookup_name, struct fsw_dnode **child_dno_out)
{
    fsw_status_t    status;
    
    status = fsw_dnode_fill(dno);
    if (status)
        return status;
    if (dno->type != FSW_DNODE_TYPE_DIR)
        return FSW_UNSUPPORTED;
    
    return dno->vol->fstype_table->dir_lookup(dno->vol, dno, lookup_name, child_dno_out);
}

fsw_status_t fsw_dnode_lookup_path(struct fsw_dnode *dno,
                                   struct fsw_string *lookup_path, char separator,
                                   struct fsw_dnode **child_dno_out)
{
    fsw_status_t    status;
    struct fsw_volume *vol = dno->vol;
    struct fsw_dnode *child_dno = NULL;
    struct fsw_string lookup_name;
    struct fsw_string remaining_path;
    int             root_if_empty;
    
    remaining_path = *lookup_path;
    fsw_dnode_retain(dno);
    
    // loop over the path
    for (root_if_empty = 1; fsw_strlen(&remaining_path) > 0; root_if_empty = 0) {
        // parse next path component
        fsw_strsplit(&lookup_name, &remaining_path, separator);
        
#if DEBUG_LEVEL
        Print(L"fsw_dnode_lookup_path: split into %d '%s' and %d '%s'\n",
              lookup_name.len, lookup_name.data,
              remaining_path.len, remaining_path.data);
#endif
        
        if (fsw_strlen(&lookup_name) == 0) {        // empty path component
            if (root_if_empty)
                child_dno = vol->root;
            else
                child_dno = dno;
            fsw_dnode_retain(child_dno);
            
        } else {
            // do an actual directory lookup
            
            // ensure we have full information
            status = fsw_dnode_fill(dno);
            if (status)
                goto errorexit;
            
            // resolve symlink if necessary
            if (dno->type == FSW_DNODE_TYPE_SYMLINK) {
                status = fsw_dnode_resolve(dno, &child_dno);
                if (status)
                    goto errorexit;
                
                // symlink target becomes the new dno
                fsw_dnode_release(dno);
                dno = child_dno;   // is already retained
                child_dno = NULL;
                
                // ensure we have full information
                status = fsw_dnode_fill(dno);
                if (status)
                    goto errorexit;
            }
            
            // make sure we operate on a directory
            if (dno->type != FSW_DNODE_TYPE_DIR) {
                return FSW_UNSUPPORTED;
                goto errorexit;
            }
            
            // check special paths
            if (fsw_streq_cstr(&lookup_name, ".")) {    // self directory
                child_dno = dno;
                fsw_dnode_retain(child_dno);
                
            } else if (fsw_streq_cstr(&lookup_name, "..")) {   // parent directory
                if (dno->parent == NULL) {
                    // We cannot go up from the root directory. Caution: Certain apps like the EFI shell
                    // rely on this behaviour!
                    status = FSW_NOT_FOUND;
                    goto errorexit;
                }
                child_dno = dno->parent;
                fsw_dnode_retain(child_dno);
                
            } else {
                // do an actual lookup
                status = vol->fstype_table->dir_lookup(vol, dno, &lookup_name, &child_dno);
                if (status)
                    goto errorexit;
            }
        }
        
        // child_dno becomes the new dno
        fsw_dnode_release(dno);
        dno = child_dno;   // is already retained
        child_dno = NULL;
        
#if DEBUG_LEVEL
        Print(L"fsw_dnode_lookup_path: now at inode %d\n", dno->dnode_id);
#endif
    }
    
    *child_dno_out = dno;
    return FSW_SUCCESS;
    
errorexit:
#if DEBUG_LEVEL
    Print(L"fsw_dnode_lookup_path: leaving with error %d\n", status);
#endif
    fsw_dnode_release(dno);
    if (child_dno != NULL)
        fsw_dnode_release(child_dno);
    return status;
}

fsw_status_t fsw_dnode_dir_read(struct fsw_shandle *shand, struct fsw_dnode **child_dno_out)
{
    struct fsw_dnode *dno = shand->dnode;
    
    if (dno->type != FSW_DNODE_TYPE_DIR)
        return FSW_UNSUPPORTED;
    
    return dno->vol->fstype_table->dir_read(dno->vol, dno, shand, child_dno_out);
}

fsw_status_t fsw_dnode_readlink(struct fsw_dnode *dno, struct fsw_string *target_name)
{
    fsw_status_t    status;
    
    status = fsw_dnode_fill(dno);
    if (status)
        return status;
    if (dno->type != FSW_DNODE_TYPE_SYMLINK)
        return FSW_UNSUPPORTED;
    
    return dno->vol->fstype_table->readlink(dno->vol, dno, target_name);
}

fsw_status_t fsw_dnode_resolve(struct fsw_dnode *dno, struct fsw_dnode **target_dno_out)
{
    fsw_status_t    status;
    struct fsw_string target_name;
    struct fsw_dnode *target_dno;
    
    fsw_dnode_retain(dno);
    
    while (1) {
        // get full information
        status = fsw_dnode_fill(dno);
        if (status)
            goto errorexit;
        if (dno->type != FSW_DNODE_TYPE_SYMLINK) {
            // found a non-symlink target, return it
            *target_dno_out = dno;
            return FSW_SUCCESS;
        }
        if (dno->parent == NULL) {    // safety measure, cannot happen in theory
            status = FSW_NOT_FOUND;
            goto errorexit;
        }
        
        // read the link's target
        status = fsw_dnode_readlink(dno, &target_name);
        if (status)
            goto errorexit;
        
        // resolve it
        status = fsw_dnode_lookup_path(dno->parent, &target_name, '/', &target_dno);
        fsw_strfree(&target_name);
        if (status)
            goto errorexit;
        
        // target_dno becomes the new dno
        fsw_dnode_release(dno);
        dno = target_dno;   // is already retained
    }
    
errorexit:
    fsw_dnode_release(dno);
    return status;
}

//
// shandle functions
//

fsw_status_t fsw_shandle_open(struct fsw_dnode *dno, struct fsw_shandle *shand)
{
    fsw_status_t    status;
    struct fsw_volume *vol = dno->vol;
    
    // read full dnode information into memory
    status = vol->fstype_table->dnode_fill(vol, dno);
    if (status)
        return status;
    
    // setup shandle
    fsw_dnode_retain(dno);
    
    shand->dnode = dno;
    shand->pos = 0;
    shand->extent.type = FSW_EXTENT_TYPE_INVALID;
    
    return FSW_SUCCESS;
}

void fsw_shandle_close(struct fsw_shandle *shand)
{
    if (shand->extent.type == FSW_EXTENT_TYPE_BUFFER)
        fsw_free(shand->extent.buffer);
    fsw_dnode_release(shand->dnode);
}

fsw_status_t fsw_shandle_read(struct fsw_shandle *shand, fsw_u32 *buffer_size_inout, void *buffer_in)
{
    fsw_status_t    status;
    struct fsw_dnode *dno = shand->dnode;
    struct fsw_volume *vol = dno->vol;
    fsw_u8          *buffer, *block_buffer;
    fsw_u32         buflen, copylen, pos;
    fsw_u32         log_bno, pos_in_extent, phys_bno, pos_in_physblock;
    
    if (shand->pos >= dno->size) {   // already at EOF
        *buffer_size_inout = 0;
        return FSW_SUCCESS;
    }
    
    // initialize vars
    buffer = buffer_in;
    buflen = *buffer_size_inout;
    pos = (fsw_u32)shand->pos;
    // restrict read to file size
    if (buflen > dno->size - pos)
        buflen = (fsw_u32)(dno->size - pos);
    
    while (buflen > 0) {
        // get extent for the current logical block
        log_bno = pos / vol->log_blocksize;
        if (shand->extent.type == FSW_EXTENT_TYPE_INVALID ||
            log_bno < shand->extent.log_start ||
            log_bno >= shand->extent.log_start + shand->extent.log_count) {
            
            if (shand->extent.type == FSW_EXTENT_TYPE_BUFFER)
                fsw_free(shand->extent.buffer);
            
            // ask the file system for the proper extent
            shand->extent.log_start = log_bno;
            status = vol->fstype_table->get_extent(vol, dno, &shand->extent);
            if (status) {
                shand->extent.type = FSW_EXTENT_TYPE_INVALID;
                return status;
            }
        }
        
        pos_in_extent = pos - shand->extent.log_start * vol->log_blocksize;
        
        // dispatch by extent type
        if (shand->extent.type == FSW_EXTENT_TYPE_PHYSBLOCK) {
            // convert to physical block number and offset
            phys_bno = shand->extent.phys_start + pos_in_extent / vol->phys_blocksize;
            pos_in_physblock = pos_in_extent & (vol->phys_blocksize - 1);
            copylen = vol->phys_blocksize - pos_in_physblock;
            if (copylen > buflen)
                copylen = buflen;
            
            // get one physical block
            status = fsw_read_block(vol, phys_bno, &block_buffer);
            if (status)
                return status;
            
            // copy data from it
            fsw_memcpy(buffer, block_buffer + pos_in_physblock, copylen);
            
        } else if (shand->extent.type == FSW_EXTENT_TYPE_BUFFER) {
            copylen = shand->extent.log_count * vol->log_blocksize - pos_in_extent;
            if (copylen > buflen)
                copylen = buflen;
            fsw_memcpy(buffer, (fsw_u8 *)shand->extent.buffer + pos_in_extent, copylen);
            
        } else {   // _SPARSE or _INVALID
            copylen = shand->extent.log_count * vol->log_blocksize - pos_in_extent;
            if (copylen > buflen)
                copylen = buflen;
            fsw_memzero(buffer, copylen);
            
        }
        
        buffer += copylen;
        buflen -= copylen;
        pos    += copylen;
    }
    
    *buffer_size_inout = (fsw_u32)(pos - shand->pos);
    shand->pos = pos;
    
    return FSW_SUCCESS;
}

//
// string functions
//

int fsw_strlen(struct fsw_string *s)
{
    if (s->type == FSW_STRING_TYPE_EMPTY)
        return 0;
    return s->len;
}

int fsw_streq(struct fsw_string *s1, struct fsw_string *s2)
{
    int i;
    struct fsw_string temp_s;
    
    // handle empty strings
    if (s1->type == FSW_STRING_TYPE_EMPTY) {
        temp_s.type = FSW_STRING_TYPE_ISO88591;
        temp_s.size = temp_s.len = 0;
        temp_s.data = NULL;
        return fsw_streq(&temp_s, s2);
    }
    if (s2->type == FSW_STRING_TYPE_EMPTY) {
        temp_s.type = FSW_STRING_TYPE_ISO88591;
        temp_s.size = temp_s.len = 0;
        temp_s.data = NULL;
        return fsw_streq(s1, &temp_s);
    }
    
    // check length (count of chars)
    if (s1->len != s2->len)
        return 0;
    if (s1->len == 0)   // both strings are empty
        return 1;
    
    if (s1->type == s2->type) {
        // same type, do a dumb memory compare
        if (s1->size != s2->size)
            return 0;
        return fsw_memeq(s1->data, s2->data, s1->size);
    }
    
    // types are different: handle all combinations, but only in one direction
    if (s1->type == FSW_STRING_TYPE_ISO88591 && s2->type == FSW_STRING_TYPE_UTF8) {
        // TODO
        return 0;
        
    } else if (s1->type == FSW_STRING_TYPE_ISO88591 && s2->type == FSW_STRING_TYPE_UTF16) {
        fsw_u8  *p1 = (fsw_u8 *) s1->data;
        fsw_u16 *p2 = (fsw_u16 *)s2->data;
        
        for (i = 0; i < s1->len; i++) {
            if (*p1++ != *p2++)
                return 0;
        }
        return 1;
        
    } else if (s1->type == FSW_STRING_TYPE_UTF8 && s2->type == FSW_STRING_TYPE_UTF16) {
        // TODO
        return 0;
        
    } else {
        // WARNING: This can create an endless recursion loop if the conditions above are
        //  not complete.
        return fsw_streq(s2, s1);
    }
}

int fsw_streq_cstr(struct fsw_string *s1, const char *s2)
{
    struct fsw_string temp_s;
    int i;
    
    for (i = 0; s2[i]; i++)
        ;
    
    temp_s.type = FSW_STRING_TYPE_ISO88591;
    temp_s.size = temp_s.len = i;
    temp_s.data = (char *)s2;
    
    return fsw_streq(s1, &temp_s);
}

fsw_status_t fsw_strdup_coerce(struct fsw_string *dest, int type, struct fsw_string *src)
{
    fsw_status_t    status;
    int             i;
    
    if (src->type == FSW_STRING_TYPE_EMPTY || src->len == 0) {
        dest->type = type;
        dest->size = dest->len = 0;
        dest->data = NULL;
        return FSW_SUCCESS;
    }
    
    if (src->type == type) {
        dest->type = type;
        dest->len  = src->len;
        dest->size = src->size;
        status = fsw_alloc(dest->size, &dest->data);
        if (status)
            return status;
        
        fsw_memcpy(dest->data, src->data, dest->size);
        return FSW_SUCCESS;
    }
    
    if (src->type == FSW_STRING_TYPE_ISO88591 && type == FSW_STRING_TYPE_UTF16) {
        fsw_u8  *sp;
        fsw_u16 *dp;
        
        dest->type = type;
        dest->len  = src->len;
        dest->size = src->len * sizeof(fsw_u16);
        status = fsw_alloc(dest->size, &dest->data);
        if (status)
            return status;
        
        sp = (fsw_u8 *) src->data;
        dp = (fsw_u16 *)dest->data;
        for (i = 0; i < src->len; i++)
            *dp++ = *sp++;
        return FSW_SUCCESS;
    }
    
    // TODO
    
    return FSW_UNSUPPORTED;
}

void fsw_strsplit(struct fsw_string *element, struct fsw_string *buffer, char separator)
{
    int i, maxlen;
    
    if (buffer->type == FSW_STRING_TYPE_EMPTY || buffer->len == 0) {
        element->type = FSW_STRING_TYPE_EMPTY;
        return;
    }
    
    maxlen = buffer->len;
    *element = *buffer;
    
    if (buffer->type == FSW_STRING_TYPE_ISO88591) {
        fsw_u8 *p;
        
        p = (fsw_u8 *)element->data;
        for (i = 0; i < maxlen; i++, p++) {
            if (*p == separator) {
                buffer->data = p + 1;
                buffer->len -= i + 1;
                break;
            }
        }
        element->len = i;
        if (i == maxlen) {
            buffer->data = p;
            buffer->len -= i;
        }
        
        element->size = element->len;
        buffer->size  = buffer->len;
        
    } else if (buffer->type == FSW_STRING_TYPE_UTF16) {
        fsw_u16 *p;
        
        p = (fsw_u16 *)element->data;
        for (i = 0; i < maxlen; i++, p++) {
            if (*p == separator) {
                buffer->data = p + 1;
                buffer->len -= i + 1;
                break;
            }
        }
        element->len = i;
        if (i == maxlen) {
            buffer->data = p;
            buffer->len -= i;
        }
        
        element->size = element->len * sizeof(fsw_u16);
        buffer->size  = buffer->len  * sizeof(fsw_u16);
        
    } else {
        // fallback
        buffer->type = FSW_STRING_TYPE_EMPTY;
    }
}

void fsw_strfree(struct fsw_string *s)
{
    if (s->type != FSW_STRING_TYPE_EMPTY && s->data)
        fsw_free(s->data);
    s->type = FSW_STRING_TYPE_EMPTY;
}

// EOF
