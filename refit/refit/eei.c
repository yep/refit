/*
 * eei.c
 * Embedded EFI Image decoder implementation
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

#include <efi.h>
#include <efilib.h>

#include "eei.h"

/*
 * Decompress RLE-encoded plane data
 */

static VOID EEIDecompress(IN const UINT8 *CompData, IN UINTN CompLen, IN UINT8 *PixelData, IN UINTN PixelCount)
{
    const UINT8 *cp;
    const UINT8 *cp_end;
    UINT8 *pp;
    UINTN pp_left;
    UINTN len, i;
    UINT8 value;
    
    cp = CompData;
    cp_end = cp + CompLen;
    pp = PixelData;
    pp_left = PixelCount;
    
    while (cp + 1 < cp_end) {
        len = *cp++;
        if (len & 0x80) {   /* compressed data: repeat next byte */
            len -= 125;
            if (len > pp_left)
                break;
            value = *cp++;
            for (i = 0; i < len; i++) {
                *pp = value;
                pp += 4;
            }
        } else {            /* uncompressed data */
            len++;
            if (len > pp_left || cp + len > cp_end)
                break;
            for (i = 0; i < len; i++) {
                *pp = *cp++;
                pp += 4;
            }
        }
        pp_left -= len;
    }
}

/*
 * Prepare an image by decompressing its pixel data
 */

EFI_STATUS EEIPrepareImage(IN EEI_IMAGE *Image)
{
    UINTN i;
    
    /* Check if we were called before */
    if (Image->PixelData != NULL)
        return EFI_SUCCESS;
    
    /* Allocate memory for the uncompressed image */
    Image->PixelData = AllocateZeroPool(Image->Width * Image->Height * 4);
    if (Image->PixelData == NULL)
        return EFI_OUT_OF_RESOURCES;
    
    /* Decompress each plane */
    for (i = 0; i < 4; i++) {
        if (Image->Planes[i].CompressedData != NULL) {
            EEIDecompress(Image->Planes[i].CompressedData, Image->Planes[i].CompressedDataLength,
                          (UINT8 *)Image->PixelData + i, Image->Width * Image->Height);
        }
        /* When there is no data for the plane, it keeps its 0x00 initialization. */
    }
    
    return EFI_SUCCESS;
}

/*
 * Release the memory allocated for an image
 */

VOID EEIFreeImage(IN EEI_IMAGE *Image)
{
    if (Image->PixelData != NULL) {
        FreePool(Image->PixelData);
        Image->PixelData = NULL;
    }
}

/* EOF */
